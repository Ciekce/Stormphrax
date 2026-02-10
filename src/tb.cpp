/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include "3rdparty/pyrrhic/tbprobe.h"
#include "move.h"

namespace stormphrax::tb {
    namespace {
        bool s_initialized{false};
    }

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

    ProbeResult probeRoot(MoveList* rootMoves, const Position& pos) {
        const auto moveFromTb = [](auto tbMove) {
            static constexpr auto kPromoPieces = std::array{
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

        const auto epSq = pos.enPassant();
        auto result = tb_probe_root_dtz(
            bbs.whiteOccupancy(),
            bbs.blackOccupancy(),
            bbs.kings(),
            bbs.queens(),
            bbs.rooks(),
            bbs.bishops(),
            bbs.knights(),
            bbs.pawns(),
            pos.halfmove(),
            epSq == Squares::kNone ? 0 : epSq.raw(),
            pos.stm() == Colors::kWhite,
            false, // TODO
            &tbRootMoves
        );

        if (!result) { // DTZ tables unavailable, fall back to WDL
            println("info string DTZ probe failed, falling back to WDL probe at root");

            result = tb_probe_root_wdl(
                bbs.whiteOccupancy(),
                bbs.blackOccupancy(),
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
            return ProbeResult::kFailed;
        }

        std::stable_sort(&tbRootMoves.moves[0], &tbRootMoves.moves[tbRootMoves.size], [](const auto& a, const auto& b) {
            return a.tbRank > b.tbRank;
        });

        const auto bestRank = tbRootMoves.moves[0].tbRank;

        const auto wdl = [&] {
            static constexpr i32 kMaxDtz = 262144;

            static constexpr i32 kWinBound = kMaxDtz - 100;
            static constexpr i32 kDrawBound = -kMaxDtz + 101;

            if (bestRank >= kWinBound) {
                return ProbeResult::kWin;
            } else if (bestRank >= kDrawBound) { // includes cursed wins and blessed losses
                return ProbeResult::kDraw;
            } else {
                return ProbeResult::kLoss;
            }
        }();

        if (!rootMoves) {
            return wdl;
        }

        for (u32 i = 0; i < tbRootMoves.size; ++i) {
            const auto& move = tbRootMoves.moves[i];

            if (move.tbRank < bestRank) {
                break;
            }

            rootMoves->push(moveFromTb(move.move));
        }

        print("info string Filtered root moves:");

        for (const auto move : *rootMoves) {
            print(" {}", move);
        }

        println();

        return wdl;
    }

    ProbeResult probe(const Position& pos) {
        const auto& bbs = pos.bbs();

        const auto epSq = pos.enPassant();
        const auto wdl = tb_probe_wdl(
            bbs.whiteOccupancy(),
            bbs.blackOccupancy(),
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
            return ProbeResult::kFailed;
        }

        if (wdl == TB_WIN) {
            return ProbeResult::kWin;
        } else if (wdl == TB_LOSS) {
            return ProbeResult::kLoss;
        } else {
            return ProbeResult::kDraw;
        }
    }
} // namespace stormphrax::tb
