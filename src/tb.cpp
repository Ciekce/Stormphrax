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

#include "3rdparty/fathom/tbprobe.h"
#include "move.h"

namespace stormphrax::tb {
    auto probeRoot(MoveList& rootMoves, const Position& pos) -> ProbeResult {
        const auto moveFromTb = [&pos](auto tbMove) {
            static constexpr auto PromoPieces =
                std::array{PieceType::None, PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};

            const auto src = static_cast<Square>(TB_MOVE_FROM(tbMove));
            const auto dst = static_cast<Square>(TB_MOVE_TO(tbMove));
            const auto promo = PromoPieces[TB_MOVE_PROMOTES(tbMove)];

            if (promo != PieceType::None) {
                return Move::promotion(src, dst, promo);
            } else if (dst == pos.enPassant() && pos.boards().pieceTypeAt(src) == PieceType::Pawn) {
                return Move::enPassant(src, dst);
            // Syzygy TBs do not encode positions with castling rights
            } else {
                return Move::standard(src, dst);
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
            0,
            epSq == Square::None ? 0 : static_cast<i32>(epSq),
            pos.toMove() == Color::White,
            false /*TODO*/,
            true,
            &tbRootMoves
        );

        if (!result) { // DTZ tables unavailable, fall back to WDL
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
                0,
                epSq == Square::None ? 0 : static_cast<i32>(epSq),
                pos.toMove() == Color::White,
                true,
                &tbRootMoves
            );
        }

        if (!result || tbRootMoves.size == 0) { // mate or stalemate at root, handled by search
            return ProbeResult::Failed;
        }

        std::sort(&tbRootMoves.moves[0], &tbRootMoves.moves[tbRootMoves.size], [](auto a, auto b) {
            return a.tbRank > b.tbRank;
        });

        const auto [wdl, minRank] = [&]() -> std::pair<ProbeResult, i32> {
            const auto best = tbRootMoves.moves[0];

            if (best.tbRank >= 900) {
                return {ProbeResult::Win, 900};
            } else if (best.tbRank >= -899) { // includes cursed wins and blessed losses
                return {ProbeResult::Draw, -899};
            } else {
                return {ProbeResult::Loss, -1000};
            }
        }();

        for (u32 i = 0; i < tbRootMoves.size; ++i) {
            const auto move = tbRootMoves.moves[i];

            if (move.tbRank < minRank) {
                break;
            }

            rootMoves.push(moveFromTb(move.move));
        }

        return wdl;
    }

    auto probe(const Position& pos) -> ProbeResult {
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
            0,
            0,
            epSq == Square::None ? 0 : static_cast<i32>(epSq),
            pos.toMove() == Color::White
        );

        if (wdl == TB_RESULT_FAILED) {
            return ProbeResult::Failed;
        }

        if (wdl == TB_WIN) {
            return ProbeResult::Win;
        } else if (wdl == TB_LOSS) {
            return ProbeResult::Loss;
        } else {
            return ProbeResult::Draw;
        }
    }
} // namespace stormphrax::tb
