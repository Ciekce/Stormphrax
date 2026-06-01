/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
 *
 * Stormphrax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stormphrax is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stormphrax. If not, see <https://www.gnu.org/licenses/>.
 */

#include "tb.h"

#include <unordered_set>
#include <vector>

#include "../3rdparty/pyrrhic/tbprobe.h"
#include "move.h"

namespace stormphrax::tb {
    namespace {
        bool s_initialized{false};

        [[nodiscard]] bool hasRepeated(const Position& pos, std::span<const u64> keys) {
            std::unordered_set<u64> keySet{};
            keySet.reserve(keys.size() + 1);
            keySet.insert(pos.key());

            for (i32 i = static_cast<i32>(keys.size()) - 1; i >= 0; --i) {
                const auto key = keys[i];

                if (keySet.contains(key)) {
                    return true;
                }

                keySet.insert(key);
            }

            return false;
        }
    } // namespace

    InitStatus init(std::string_view path) {
        const std::string pathStr{path};

        if (!tb_init(pathStr.c_str())) {
            eprintln("failed to initialize Pyrrhic");
            return InitStatus::kFailed;
        }

        println("info string Found {} WDL and {} DTZ files up to {}-man", TB_NUM_WDL, TB_NUM_DTZ, TB_LARGEST);

        return TB_LARGEST == 0 ? InitStatus::kNoneFound : InitStatus::kSuccess;
    }

    void free() {
        if (s_initialized) {
            tb_free();
            s_initialized = false;
        }
    }

    std::pair<search::GameResult, bool> probeRoot(
        const Position& pos,
        std::span<const u64> keys,
        std::span<search::RootMove> rootMoves
    ) {
        const auto moveFromTb = [](auto tbMove) {
            static constexpr std::array kPromoPieces = {
                PieceTypes::kNone,
                PieceTypes::kQueen,
                PieceTypes::kRook,
                PieceTypes::kBishop,
                PieceTypes::kKnight,
            };

            const auto from = Square::fromRaw(PYRRHIC_MOVE_FROM(tbMove));
            const auto to = Square::fromRaw(PYRRHIC_MOVE_TO(tbMove));
            const auto promo = kPromoPieces[PYRRHIC_MOVE_FLAGS(tbMove) & 0x7];

            if (PYRRHIC_MOVE_IS_ENPASS(tbMove)) {
                return Move::enPassant(from, to);
            } else if (promo != PieceTypes::kNone) {
                return Move::promotion(from, to, promo);
            } else {
                return Move::standard(from, to);
            }
        };

        const auto& bbs = pos.bbs();

        TbRootMoves tbRootMoves{};
        bool dtzSucceeded = true;

        const auto epSq = pos.enPassant();
        auto result = tb_probe_root_dtz(
            bbs.white(),
            bbs.black(),
            bbs.kings(),
            bbs.queens(),
            bbs.rooks(),
            bbs.bishops(),
            bbs.knights(),
            bbs.pawns(),
            pos.halfmove(),
            epSq == Squares::kNone ? 0 : epSq.raw(),
            pos.stm() == Colors::kWhite,
            hasRepeated(pos, keys),
            &tbRootMoves
        );

        if (!result) { // DTZ tables unavailable, fall back to WDL
            println("info string DTZ probe failed, falling back to WDL probe at root");

            dtzSucceeded = false;

            result = tb_probe_root_wdl(
                bbs.white(),
                bbs.black(),
                bbs.kings(),
                bbs.queens(),
                bbs.rooks(),
                bbs.bishops(),
                bbs.knights(),
                bbs.pawns(),
                pos.halfmove(),
                epSq == Squares::kNone ? 0 : epSq.raw(),
                pos.stm() == Colors::kWhite,
                true,
                &tbRootMoves
            );

            if (!result) {
                println("info string WDL probe failed");
            }
        }

        if (!result || tbRootMoves.size == 0) { // mate or stalemate at root, handled by search
            return {search::GameResult::kNone, dtzSucceeded};
        }

        std::vector<u64> keyStack{};
        keyStack.reserve(keys.size() + 1);
        keyStack.assign(keys.begin(), keys.end());
        keyStack.push_back(pos.key());

        // Correct any moves that immediately threefold
        for (u32 idx = 0; idx < tbRootMoves.size; ++idx) {
            const auto move = moveFromTb(tbRootMoves.moves[idx].move);
            const auto posAfter = pos.applyMove(move);
            if (posAfter.isDrawnByRepetition(0, keyStack)) {
                tbRootMoves.moves[idx].tbRank = 0;
            }
        }

        std::sort(
            tbRootMoves.moves,
            tbRootMoves.moves + tbRootMoves.size,
            [](const TbRootMove& a, const TbRootMove& b) { return a.tbRank > b.tbRank; }
        );

        const auto isTwofold = [&](Move move) {
            const auto posAfter = pos.applyMove(move);
            return posAfter.isDrawnByRepetition(999999, keyStack);
        };

        const auto toWdl = [](i32 rank) {
            static constexpr i32 kMaxDtz = 262144;

            static constexpr i32 kWinBound = kMaxDtz - 100;
            static constexpr i32 kDrawBound = -kMaxDtz + 101;

            if (rank >= kWinBound) {
                return search::GameResult::kWin;
            } else if (rank >= kDrawBound) { // includes cursed wins and blessed losses
                return search::GameResult::kDraw;
            } else {
                return search::GameResult::kLoss;
            }
        };

        if (rootMoves.empty()) {
            const auto bestWdl = toWdl(tbRootMoves.moves[0].tbRank);
            return {bestWdl, dtzSucceeded};
        }

        const auto getRootMove = [&](Move move) -> search::RootMove* {
            for (auto& rootMove : rootMoves) {
                if (rootMove.move() == move) {
                    return &rootMove;
                }
            }

            return nullptr;
        };

        for (u32 i = 0; i < tbRootMoves.size; ++i) {
            const auto [tbMove, tbRank] = tbRootMoves.moves[i];

            const auto move = moveFromTb(tbMove);
            auto* rootMove = getRootMove(move);

            if (!rootMove) {
                // excluded by searchmoves
                continue;
            }

            const auto wdl = toWdl(tbRank);

            auto adjustedRank = tbRank * 2;
            if (wdl == search::GameResult::kWin && isTwofold(move)) {
                adjustedRank -= 1;
            }

            rootMove->setTbStatus(wdl, adjustedRank);
        }

        std::ranges::stable_sort(rootMoves, [](const search::RootMove& a, const search::RootMove& b) {
            return a.tbRank > b.tbRank;
        });

        return {rootMoves[0].tbWdl, dtzSucceeded};
    }

    search::GameResult probeWdl(const Position& pos) {
        const auto& bbs = pos.bbs();

        const auto epSq = pos.enPassant();
        const auto wdl = tb_probe_wdl(
            bbs.white(),
            bbs.black(),
            bbs.kings(),
            bbs.queens(),
            bbs.rooks(),
            bbs.bishops(),
            bbs.knights(),
            bbs.pawns(),
            epSq == Squares::kNone ? 0 : epSq.raw(),
            pos.stm() == Colors::kWhite
        );

        if (wdl == TB_RESULT_FAILED) {
            return search::GameResult::kNone;
        }

        if (wdl == TB_WIN) {
            return search::GameResult::kWin;
        } else if (wdl == TB_LOSS) {
            return search::GameResult::kLoss;
        } else {
            return search::GameResult::kDraw;
        }
    }
} // namespace stormphrax::tb
