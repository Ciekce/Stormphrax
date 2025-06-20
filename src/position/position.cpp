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

#include "position.h"

#include <algorithm>
#include <cassert>

#include "../attacks/attacks.h"
#include "../cuckoo.h"
#include "../movegen.h"
#include "../opts.h"
#include "../rays.h"
#include "../util/parse.h"
#include "../util/split.h"

namespace stormphrax {
    namespace {
        std::array<PieceType, 8> scharnaglToBackrank(u32 n) {
            // https://en.wikipedia.org/wiki/Fischer_random_chess_numbering_scheme#Direct_derivation

            // these are stored with the second knight moved left by an empty square,
            // because the first knight fills a square before the second knight is placed
            static constexpr auto kN5n = std::array{
                std::pair{0, 0},
                std::pair{0, 1},
                std::pair{0, 2},
                std::pair{0, 3},
                std::pair{1, 1},
                std::pair{1, 2},
                std::pair{1, 3},
                std::pair{2, 2},
                std::pair{2, 3},
                std::pair{3, 3}
            };

            assert(n < 960);

            std::array<PieceType, 8> dst{};
            // no need to fill with empty pieces, because pawns are impossible

            const auto placeInNthFree = [&dst](u32 n, PieceType piece) {
                for (i32 free = 0, i = 0; i < 8; ++i) {
                    if (dst[i] == PieceType::kPawn && free++ == n) {
                        dst[i] = piece;
                        break;
                    }
                }
            };

            const auto placeInFirstFree = [&dst](PieceType piece) {
                for (i32 i = 0; i < 8; ++i) {
                    if (dst[i] == PieceType::kPawn) {
                        dst[i] = piece;
                        break;
                    }
                }
            };

            const auto n2 = n / 4;
            const auto b1 = n % 4;

            const auto n3 = n2 / 4;
            const auto b2 = n2 % 4;

            const auto n4 = n3 / 6;
            const auto q = n3 % 6;

            dst[b1 * 2 + 1] = PieceType::kBishop;
            dst[b2 * 2] = PieceType::kBishop;

            placeInNthFree(q, PieceType::kQueen);

            const auto [knight1, knight2] = kN5n[n4];

            placeInNthFree(knight1, PieceType::kKnight);
            placeInNthFree(knight2, PieceType::kKnight);

            placeInFirstFree(PieceType::kRook);
            placeInFirstFree(PieceType::kKing);
            placeInFirstFree(PieceType::kRook);

            return dst;
        }
    } // namespace

    template Position Position::applyMove<NnueUpdateAction::kNone>(Move, eval::NnueState*) const;
    template Position Position::applyMove<NnueUpdateAction::kQueue>(Move, eval::NnueState*) const;
    template Position Position::applyMove<NnueUpdateAction::kApply>(Move, eval::NnueState*) const;

    template void Position::setPiece<false>(Piece, Square);
    template void Position::setPiece<true>(Piece, Square);

    template void Position::removePiece<false>(Piece, Square);
    template void Position::removePiece<true>(Piece, Square);

    template void Position::movePieceNoCap<false>(Piece, Square, Square);
    template void Position::movePieceNoCap<true>(Piece, Square, Square);

    template Piece Position::movePiece<false, false>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::movePiece<true, false>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::movePiece<false, true>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::movePiece<true, true>(Piece, Square, Square, eval::NnueUpdates&);

    template Piece Position::promotePawn<false, false>(Piece, Square, Square, PieceType, eval::NnueUpdates&);
    template Piece Position::promotePawn<true, false>(Piece, Square, Square, PieceType, eval::NnueUpdates&);
    template Piece Position::promotePawn<false, true>(Piece, Square, Square, PieceType, eval::NnueUpdates&);
    template Piece Position::promotePawn<true, true>(Piece, Square, Square, PieceType, eval::NnueUpdates&);

    template void Position::castle<false, false>(Piece, Square, Square, eval::NnueUpdates&);
    template void Position::castle<true, false>(Piece, Square, Square, eval::NnueUpdates&);
    template void Position::castle<false, true>(Piece, Square, Square, eval::NnueUpdates&);
    template void Position::castle<true, true>(Piece, Square, Square, eval::NnueUpdates&);

    template Piece Position::enPassant<false, false>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::enPassant<true, false>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::enPassant<false, true>(Piece, Square, Square, eval::NnueUpdates&);
    template Piece Position::enPassant<true, true>(Piece, Square, Square, eval::NnueUpdates&);

    template <NnueUpdateAction kNnueAction>
    Position Position::applyMove(Move move, eval::NnueState* nnueState) const {
        static constexpr bool kUpdateNnue = kNnueAction != NnueUpdateAction::kNone;

        if constexpr (kUpdateNnue) {
            assert(nnueState != nullptr);
        }

        auto newPos = *this;

        newPos.m_stm = oppColor(m_stm);
        newPos.m_keys.flipStm();

        if (newPos.m_enPassant != Square::kNone) {
            newPos.m_keys.flipEp(newPos.m_enPassant);
            newPos.m_enPassant = Square::kNone;
        }

        const auto stm = newPos.nstm();
        const auto nstm = oppColor(stm);

        if (stm == Color::kBlack) {
            ++newPos.m_fullmove;
        }

        if (!move) {
            newPos.m_pinned = newPos.calcPinned();
            newPos.m_threats = newPos.calcThreats();

            return newPos;
        }

        const auto moveType = move.type();

        const auto moveSrc = move.fromSq();
        const auto moveDst = move.toSq();

        const auto moving = m_boards.pieceOn(moveSrc);
        const auto movingType = pieceType(moving);

        eval::NnueUpdates updates{};
        auto captured = Piece::kNone;

        switch (moveType) {
            case MoveType::kStandard:
                captured = newPos.movePiece<true, kUpdateNnue>(moving, moveSrc, moveDst, updates);
                break;
            case MoveType::kPromotion:
                captured = newPos.promotePawn<true, kUpdateNnue>(moving, moveSrc, moveDst, move.promo(), updates);
                break;
            case MoveType::kCastling:
                newPos.castle<true, kUpdateNnue>(moving, moveSrc, moveDst, updates);
                break;
            case MoveType::kEnPassant:
                captured = newPos.enPassant<true, kUpdateNnue>(moving, moveSrc, moveDst, updates);
                break;
        }

        assert(pieceTypeOrNone(captured) != PieceType::kKing);

        if constexpr (kUpdateNnue) {
            nnueState->pushUpdates<kNnueAction == NnueUpdateAction::kApply>(updates, m_boards.bbs(), m_kings);
        }

        if (movingType == PieceType::kRook) {
            newPos.m_castlingRooks.color(stm).unset(moveSrc);
        } else if (movingType == PieceType::kKing) {
            newPos.m_castlingRooks.color(stm).clear();
        } else if (moving == Piece::kBlackPawn && move.fromSqRank() == 6 && move.toSqRank() == 4) {
            newPos.m_enPassant = toSquare(5, move.fromSqFile());
            newPos.m_keys.flipEp(newPos.m_enPassant);
        } else if (moving == Piece::kWhitePawn && move.fromSqRank() == 1 && move.toSqRank() == 3) {
            newPos.m_enPassant = toSquare(2, move.fromSqFile());
            newPos.m_keys.flipEp(newPos.m_enPassant);
        }

        if (captured == Piece::kNone && pieceType(moving) != PieceType::kPawn) {
            ++newPos.m_halfmove;
        } else {
            newPos.m_halfmove = 0;
        }

        if (captured != Piece::kNone && pieceType(captured) == PieceType::kRook) {
            newPos.m_castlingRooks.color(nstm).unset(moveDst);
        }

        if (newPos.m_castlingRooks != m_castlingRooks) {
            newPos.m_keys.switchCastling(m_castlingRooks, newPos.m_castlingRooks);
            newPos.m_castlingRooks = newPos.m_castlingRooks;
        }

        newPos.m_checkers = newPos.calcCheckers();
        newPos.m_pinned = newPos.calcPinned();
        newPos.m_threats = newPos.calcThreats();

        newPos.filterEp(nstm);

        return newPos;
    }

    bool Position::isPseudolegal(Move move) const {
        assert(move != kNullMove);

        const auto us = stm();

        const auto src = move.fromSq();
        const auto srcPiece = m_boards.pieceOn(src);

        if (srcPiece == Piece::kNone || pieceColor(srcPiece) != us) {
            return false;
        }

        const auto type = move.type();

        const auto dst = move.toSq();
        const auto dstPiece = m_boards.pieceOn(dst);

        // we're capturing something
        if (dstPiece != Piece::kNone
            // we're capturing our own piece
            && ((pieceColor(dstPiece) == us
                 //  and either not castling
                 && (type != MoveType::kCastling
                     // or trying to castle with a non-rook
                     || dstPiece != colorPiece(PieceType::kRook, us)))
                // or trying to capture a king
                || pieceType(dstPiece) == PieceType::kKing))
        {
            return false;
        }

        const auto srcPieceType = pieceType(srcPiece);
        const auto them = oppColor(us);
        const auto occ = m_boards.bbs().occupancy();

        if (type == MoveType::kCastling) {
            if (srcPieceType != PieceType::kKing || isCheck()) {
                return false;
            }

            const auto homeRank = relativeRank(us, 0);

            // wrong rank
            if (move.fromSqRank() != homeRank || move.toSqRank() != homeRank) {
                return false;
            }

            const auto rank = squareRank(src);

            Square kingDst, rookDst;

            if (squareFile(src) < squareFile(dst)) {
                // no castling rights
                if (dst != m_castlingRooks.color(us).kingside) {
                    return false;
                }

                kingDst = toSquare(rank, 6);
                rookDst = toSquare(rank, 5);
            } else {
                // no castling rights
                if (dst != m_castlingRooks.color(us).queenside) {
                    return false;
                }

                kingDst = toSquare(rank, 2);
                rookDst = toSquare(rank, 3);
            }

            // same checks as for movegen
            if (g_opts.chess960) {
                const auto toKingDst = rayBetween(src, kingDst);
                const auto toRook = rayBetween(src, dst);

                const auto castleOcc = occ ^ squareBit(src) ^ squareBit(dst);

                return (castleOcc & (toKingDst | toRook | squareBit(kingDst) | squareBit(rookDst))).empty()
                    && !anyAttacked(toKingDst | squareBit(kingDst), them);
            } else {
                if (dst == m_castlingRooks.black().kingside) {
                    return (occ & U64(0x6000000000000000)).empty() && !isAttacked(Square::kF8, Color::kWhite);
                } else if (dst == m_castlingRooks.black().queenside) {
                    return (occ & U64(0x0E00000000000000)).empty() && !isAttacked(Square::kD8, Color::kWhite);
                } else if (dst == m_castlingRooks.white().kingside) {
                    return (occ & U64(0x0000000000000060)).empty() && !isAttacked(Square::kF1, Color::kBlack);
                } else {
                    return (occ & U64(0x000000000000000E)).empty() && !isAttacked(Square::kD1, Color::kBlack);
                }
            }
        }

        if (srcPieceType == PieceType::kPawn) {
            if (type == MoveType::kEnPassant) {
                return dst == m_enPassant && attacks::getPawnAttacks(m_enPassant, them)[src];
            }

            const auto srcRank = move.fromSqRank();
            const auto dstRank = move.toSqRank();

            // backwards move
            if ((us == Color::kBlack && dstRank >= srcRank) || (us == Color::kWhite && dstRank <= srcRank)) {
                return false;
            }

            const auto promoRank = relativeRank(us, 7);

            // non-promotion move to back rank, or promotion move to any other rank
            if ((type == MoveType::kPromotion) != (dstRank == promoRank)) {
                return false;
            }

            // sideways move
            if (move.fromSqFile() != move.toSqFile()) {
                // not valid attack
                if (!(attacks::getPawnAttacks(src, us) & m_boards.bbs().forColor(them))[dst]) {
                    return false;
                }
            } else if (dstPiece != Piece::kNone) {
                // forward move onto a piece
                return false;
            }

            const auto delta = std::abs(dstRank - srcRank);

            i32 maxDelta;
            if (us == Color::kBlack) {
                maxDelta = srcRank == 6 ? 2 : 1;
            } else {
                maxDelta = srcRank == 1 ? 2 : 1;
            }

            if (delta > maxDelta) {
                return false;
            }

            if (delta == 2
                && occ[static_cast<Square>(
                    static_cast<i32>(dst) + (us == Color::kWhite ? offsets::kDown : offsets::kUp)
                )])
            {
                return false;
            }
        } else {
            if (type == MoveType::kPromotion || type == MoveType::kEnPassant) {
                return false;
            }

            Bitboard attacks{};

            switch (srcPieceType) {
                case PieceType::kKnight:
                    attacks = attacks::getKnightAttacks(src);
                    break;
                case PieceType::kBishop:
                    attacks = attacks::getBishopAttacks(src, occ);
                    break;
                case PieceType::kRook:
                    attacks = attacks::getRookAttacks(src, occ);
                    break;
                case PieceType::kQueen:
                    attacks = attacks::getQueenAttacks(src, occ);
                    break;
                case PieceType::kKing:
                    attacks = attacks::getKingAttacks(src);
                    break;
                default:
                    __builtin_unreachable();
            }

            if (!attacks[dst]) {
                return false;
            }
        }

        return true;
    }

    // This does *not* check for pseudolegality, moves are assumed to be pseudolegal
    bool Position::isLegal(Move move) const {
        assert(move != kNullMove);

        const auto us = stm();
        const auto them = oppColor(us);

        const auto& bbs = m_boards.bbs();

        const auto src = move.fromSq();
        const auto dst = move.toSq();

        const auto king = m_kings.color(us);

        if (move.type() == MoveType::kCastling) {
            const auto kingDst = toSquare(move.fromSqRank(), move.fromSqFile() < move.toSqFile() ? 6 : 2);
            return !m_threats[kingDst] && !(g_opts.chess960 && pinned(us)[dst]);
        } else if (move.type() == MoveType::kEnPassant) {
            auto rank = squareRank(dst);
            const auto file = squareFile(dst);

            rank = rank == 2 ? 3 : 4;

            const auto captureSquare = toSquare(rank, file);

            const auto postEpOcc = bbs.occupancy() ^ Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst)
                                 ^ Bitboard::fromSquare(captureSquare);

            const auto theirQueens = bbs.queens(them);

            return (attacks::getBishopAttacks(king, postEpOcc) & (theirQueens | bbs.bishops(them))).empty()
                && (attacks::getRookAttacks(king, postEpOcc) & (theirQueens | bbs.rooks(them))).empty();
        }

        const auto moving = m_boards.pieceOn(src);

        if (pieceType(moving) == PieceType::kKing) {
            const auto kinglessOcc = bbs.occupancy() ^ bbs.kings(us);
            const auto theirQueens = bbs.queens(them);

            return !m_threats[move.toSq()]
                && (attacks::getBishopAttacks(dst, kinglessOcc) & (theirQueens | bbs.bishops(them))).empty()
                && (attacks::getRookAttacks(dst, kinglessOcc) & (theirQueens | bbs.rooks(them))).empty();
        }

        // multiple checks can only be evaded with a king move
        if (m_checkers.multiple() || pinned(us)[src] && !rayIntersecting(src, dst)[king]) {
            return false;
        }

        if (m_checkers.empty()) {
            return true;
        }

        const auto checker = m_checkers.lowestSquare();
        return (rayBetween(king, checker) | Bitboard::fromSquare(checker))[dst];
    }

    // see comment in cuckoo.cpp
    bool Position::hasCycle(i32 ply, std::span<const u64> keys) const {
        const auto end = std::min<i32>(m_halfmove, static_cast<i32>(keys.size()));

        if (end < 3) {
            return false;
        }

        const auto S = [&](i32 d) { return keys[keys.size() - d]; };

        const auto occ = m_boards.bbs().occupancy();
        const auto originalKey = m_keys.all;

        auto other = ~(originalKey ^ S(1));

        for (i32 d = 3; d <= end; d += 2) {
            const auto currKey = S(d);

            other ^= ~(currKey ^ S(d - 1));
            if (other != 0) {
                continue;
            }

            const auto diff = originalKey ^ currKey;

            u32 slot = cuckoo::h1(diff);

            if (diff != cuckoo::keys[slot]) {
                slot = cuckoo::h2(diff);
            }

            if (diff != cuckoo::keys[slot]) {
                continue;
            }

            const auto move = cuckoo::moves[slot];

            if ((occ & rayBetween(move.fromSq(), move.toSq())).empty()) {
                // repetition is after root, done
                if (ply > d) {
                    return true;
                }

                auto piece = m_boards.pieceOn(move.fromSq());
                if (piece == Piece::kNone) {
                    piece = m_boards.pieceOn(move.toSq());
                }

                assert(piece != Piece::kNone);

                return pieceColor(piece) == stm();
            }
        }

        return false;
    }

    bool Position::isDrawn(i32 ply, std::span<const u64> keys) const {
        const auto halfmove = m_halfmove;

        if (halfmove >= 100) {
            if (!isCheck()) {
                return true;
            }

            //TODO there's a speedup possible here, but
            // it requires a lot of movegen refactoring
            ScoredMoveList moves{};
            generateAll(moves, *this);

            return std::ranges::any_of(moves, [this](const auto move) { return isLegal(move.move); });
        }

        const auto currKey = m_keys.all;
        const auto limit = std::max(0, static_cast<i32>(keys.size()) - halfmove - 2);

        ply -= 4;

        i32 repetitions = 0;

        for (auto i = static_cast<i32>(keys.size()) - 4; i >= limit; i -= 2, ply -= 2) {
            // require a threefold repetition before root
            if (keys[i] == currKey && ++repetitions == 1 + (ply < 0)) {
                return true;
            }
        }

        const auto& bbs = this->bbs();

        if (!bbs.pawns().empty() || !bbs.majors().empty()) {
            return false;
        }

        // KK
        if (bbs.nonPk().empty()) {
            return true;
        }

        // KNK or KBK
        if ((bbs.blackNonPk().empty() && bbs.whiteNonPk() == bbs.whiteMinors() && !bbs.whiteMinors().multiple())
            || (bbs.whiteNonPk().empty() && bbs.blackNonPk() == bbs.blackMinors() && !bbs.blackMinors().multiple()))
        {
            return true;
        }

        // KBKB OCB
        if ((bbs.blackNonPk() == bbs.blackBishops() && bbs.whiteNonPk() == bbs.whiteBishops())
            && !bbs.blackBishops().multiple() && !bbs.whiteBishops().multiple()
            && (bbs.blackBishops() & boards::kLightSquares).empty()
                   != (bbs.whiteBishops() & boards::kLightSquares).empty())
        {
            return true;
        }

        return false;
    }

    std::string Position::toFen() const {
        std::string fen{};
        auto itr = std::back_inserter(fen);

        for (i32 rank = 7; rank >= 0; --rank) {
            for (i32 file = 0; file < 8; ++file) {
                if (m_boards.pieceAt(rank, file) == Piece::kNone) {
                    u32 emptySquares = 1;
                    for (; file < 7 && m_boards.pieceAt(rank, file + 1) == Piece::kNone; ++file, ++emptySquares) {
                    }

                    fmt::format_to(itr, "{}", static_cast<char>('0' + emptySquares));
                } else {
                    fmt::format_to(itr, "{}", m_boards.pieceAt(rank, file));
                }
            }

            if (rank > 0) {
                fmt::format_to(itr, "/");
            }
        }

        fmt::format_to(itr, "{}", stm() == Color::kWhite ? " w " : " b ");

        if (m_castlingRooks == CastlingRooks{}) {
            fmt::format_to(itr, "-");
        } else if (g_opts.chess960) {
            constexpr auto kBlackFiles = std::array{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
            constexpr auto kWhiteFiles = std::array{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

            if (m_castlingRooks.white().kingside != Square::kNone) {
                fmt::format_to(itr, "{}", kWhiteFiles[squareFile(m_castlingRooks.white().kingside)]);
            }
            if (m_castlingRooks.white().queenside != Square::kNone) {
                fmt::format_to(itr, "{}", kWhiteFiles[squareFile(m_castlingRooks.white().queenside)]);
            }
            if (m_castlingRooks.black().kingside != Square::kNone) {
                fmt::format_to(itr, "{}", kBlackFiles[squareFile(m_castlingRooks.black().kingside)]);
            }
            if (m_castlingRooks.black().queenside != Square::kNone) {
                fmt::format_to(itr, "{}", kBlackFiles[squareFile(m_castlingRooks.black().queenside)]);
            }
        } else {
            if (m_castlingRooks.white().kingside != Square::kNone) {
                fmt::format_to(itr, "K");
            }
            if (m_castlingRooks.white().queenside != Square::kNone) {
                fmt::format_to(itr, "Q");
            }
            if (m_castlingRooks.black().kingside != Square::kNone) {
                fmt::format_to(itr, "k");
            }
            if (m_castlingRooks.black().queenside != Square::kNone) {
                fmt::format_to(itr, "q");
            }
        }

        if (m_enPassant != Square::kNone) {
            fmt::format_to(itr, " {}", m_enPassant);
        } else {
            fmt::format_to(itr, " -");
        }

        fmt::format_to(itr, " {} {}", m_halfmove, m_fullmove);

        return fen;
    }

    template <bool kUpdateKey>
    void Position::setPiece(Piece piece, Square square) {
        assert(piece != Piece::kNone);
        assert(square != Square::kNone);

        assert(pieceType(piece) != PieceType::kKing);

        m_boards.setPiece(square, piece);

        if constexpr (kUpdateKey) {
            m_keys.flipPiece(piece, square);
        }
    }

    template <bool kUpdateKey>
    void Position::removePiece(Piece piece, Square square) {
        assert(piece != Piece::kNone);
        assert(square != Square::kNone);

        assert(pieceType(piece) != PieceType::kKing);

        m_boards.removePiece(square, piece);

        if constexpr (kUpdateKey) {
            m_keys.flipPiece(piece, square);
        }
    }

    template <bool kUpdateKey>
    void Position::movePieceNoCap(Piece piece, Square src, Square dst) {
        assert(piece != Piece::kNone);

        assert(src != Square::kNone);
        assert(dst != Square::kNone);

        if (src == dst) {
            return;
        }

        m_boards.movePiece(src, dst, piece);

        if (pieceType(piece) == PieceType::kKing) {
            const auto color = pieceColor(piece);
            m_kings.color(color) = dst;
        }

        if constexpr (kUpdateKey) {
            m_keys.movePiece(piece, src, dst);
        }
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    Piece Position::movePiece(Piece piece, Square src, Square dst, eval::NnueUpdates& nnueUpdates) {
        assert(piece != Piece::kNone);

        assert(src != Square::kNone);
        assert(dst != Square::kNone);
        assert(src != dst);

        const auto captured = m_boards.pieceOn(dst);

        if (captured != Piece::kNone) {
            assert(pieceType(captured) != PieceType::kKing);

            m_boards.removePiece(dst, captured);

            // NNUE update done below

            if constexpr (kUpdateKey) {
                m_keys.flipPiece(captured, dst);
            }
        }

        m_boards.movePiece(src, dst, piece);

        if (pieceType(piece) == PieceType::kKing) {
            const auto color = pieceColor(piece);

            if constexpr (kUpdateNnue) {
                if (eval::InputFeatureSet::refreshRequired(color, m_kings.color(color), dst)) {
                    nnueUpdates.setRefresh(color);
                }
            }

            m_kings.color(color) = dst;
        }

        if constexpr (kUpdateNnue) {
            nnueUpdates.pushSubAdd(piece, src, dst);

            if (captured != Piece::kNone) {
                nnueUpdates.pushSub(captured, dst);
            }
        }

        if constexpr (kUpdateKey) {
            m_keys.movePiece(piece, src, dst);
        }

        return captured;
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    Piece Position::promotePawn(Piece pawn, Square src, Square dst, PieceType promo, eval::NnueUpdates& nnueUpdates) {
        assert(pawn != Piece::kNone);
        assert(pieceType(pawn) == PieceType::kPawn);

        assert(src != Square::kNone);
        assert(dst != Square::kNone);
        assert(src != dst);

        assert(squareRank(dst) == relativeRank(pieceColor(pawn), 7));
        assert(squareRank(src) == relativeRank(pieceColor(pawn), 6));

        assert(promo != PieceType::kNone);

        const auto captured = m_boards.pieceOn(dst);

        if (captured != Piece::kNone) {
            assert(pieceType(captured) != PieceType::kKing);

            m_boards.removePiece(dst, captured);

            if constexpr (kUpdateNnue) {
                nnueUpdates.pushSub(captured, dst);
            }

            if constexpr (kUpdateKey) {
                m_keys.flipPiece(captured, dst);
            }
        }

        m_boards.moveAndChangePiece(src, dst, pawn, promo);

        if constexpr (kUpdateNnue || kUpdateKey) {
            const auto coloredPromo = copyPieceColor(pawn, promo);

            if constexpr (kUpdateNnue) {
                nnueUpdates.pushSub(pawn, src);
                nnueUpdates.pushAdd(coloredPromo, dst);
            }

            if constexpr (kUpdateKey) {
                m_keys.flipPiece(pawn, src);
                m_keys.flipPiece(coloredPromo, dst);
            }
        }

        return captured;
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    void Position::castle(Piece king, Square kingSrc, Square rookSrc, eval::NnueUpdates& nnueUpdates) {
        assert(king != Piece::kNone);
        assert(pieceType(king) == PieceType::kKing);

        assert(kingSrc != Square::kNone);
        assert(rookSrc != Square::kNone);
        assert(kingSrc != rookSrc);

        const auto rank = squareRank(kingSrc);

        Square kingDst, rookDst;

        if (squareFile(kingSrc) < squareFile(rookSrc)) {
            // short
            kingDst = toSquare(rank, 6);
            rookDst = toSquare(rank, 5);
        } else {
            // long
            kingDst = toSquare(rank, 2);
            rookDst = toSquare(rank, 3);
        }

        const auto rook = copyPieceColor(king, PieceType::kRook);

        movePieceNoCap<kUpdateKey>(king, kingSrc, kingDst);
        movePieceNoCap<kUpdateKey>(rook, rookSrc, rookDst);

        if constexpr (kUpdateNnue) {
            const auto color = pieceColor(king);

            if (eval::InputFeatureSet::refreshRequired(color, kingSrc, kingDst)) {
                nnueUpdates.setRefresh(color);
            }

            nnueUpdates.pushSubAdd(king, kingSrc, kingDst);
            nnueUpdates.pushSubAdd(rook, rookSrc, rookDst);
        }
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    Piece Position::enPassant(Piece pawn, Square src, Square dst, eval::NnueUpdates& nnueUpdates) {
        assert(pawn != Piece::kNone);
        assert(pieceType(pawn) == PieceType::kPawn);

        assert(src != Square::kNone);
        assert(dst != Square::kNone);
        assert(src != dst);

        m_boards.movePiece(src, dst, pawn);

        if constexpr (kUpdateNnue) {
            nnueUpdates.pushSubAdd(pawn, src, dst);
        }

        if constexpr (kUpdateKey) {
            m_keys.movePiece(pawn, src, dst);
        }

        auto rank = squareRank(dst);
        const auto file = squareFile(dst);

        rank = rank == 2 ? 3 : 4;

        const auto captureSquare = toSquare(rank, file);
        const auto enemyPawn = flipPieceColor(pawn);

        m_boards.removePiece(captureSquare, enemyPawn);

        if constexpr (kUpdateNnue) {
            nnueUpdates.pushSub(enemyPawn, captureSquare);
        }

        if constexpr (kUpdateKey) {
            m_keys.flipPiece(enemyPawn, captureSquare);
        }

        return enemyPawn;
    }

    void Position::regen() {
        m_boards.regenFromBbs();

        m_keys.clear();

        for (u32 rank = 0; rank < 8; ++rank) {
            for (u32 file = 0; file < 8; ++file) {
                const auto square = toSquare(rank, file);
                if (const auto piece = m_boards.pieceOn(square); piece != Piece::kNone) {
                    if (pieceType(piece) == PieceType::kKing) {
                        m_kings.color(pieceColor(piece)) = square;
                    }

                    m_keys.flipPiece(piece, toSquare(rank, file));
                }
            }
        }

        m_keys.flipCastling(m_castlingRooks);
        m_keys.flipEp(m_enPassant);

        if (stm() == Color::kBlack) {
            m_keys.flipStm();
        }

        m_checkers = calcCheckers();
        m_pinned = calcPinned();
        m_threats = calcThreats();

        filterEp(stm());
    }

    void Position::filterEp(Color capturing) {
        if (m_enPassant == Square::kNone) {
            return;
        }

        const auto unset = [this] {
            m_keys.flipEp(m_enPassant);
            m_enPassant = Square::kNone;
        };

        const auto& bbs = m_boards.bbs();

        const auto moved = oppColor(capturing);

        const auto king = m_kings.color(capturing);

        const auto pinnedPieces = pinned(capturing);
        auto candidates = bbs.pawns(capturing) & attacks::getPawnAttacks(m_enPassant, moved);

        // vertically pinned pawns cannot capture at all
        const auto vertPinned = pinnedPieces & boards::kFiles[squareFile(king)];
        candidates &= ~vertPinned;

        if (!candidates) {
            unset();
            return;
        }

        const auto diagPinned = candidates & pinnedPieces;

        if (candidates.multiple()) {
            // if there are two diagonally pinned pawns, neither can possibly capture
            if (candidates == diagPinned) {
                unset();
            }

            // otherwise, one pawn has to be unpinned, and thus ep is legal.
            // the discovered check case handled below cannot apply -
            // the other pawn will still block the potential check.

            // either way, we can stop here
            return;
        }

        // if the capturing pawn is pinned, it has to be pinned
        // along the same diagonal that the capture would occur
        if (diagPinned) {
            const auto pinnedPawn = diagPinned.lowestSquare();
            const auto pinRay =
                attacks::getBishopAttacks(king, bbs.occupancy(moved)) & rayIntersecting(king, pinnedPawn);

            if (!pinRay[m_enPassant]) {
                unset();
                return;
            }
        }

        // also handle the annoying case where capturing en passant would cause discovered check
        const auto movedPawn =
            toSquare(squareRank(m_enPassant) + (moved == Color::kWhite ? 1 : -1), squareFile(m_enPassant));
        const auto capturingPawn = candidates.lowestSquare();

        const auto rank = rayIntersecting(movedPawn, capturingPawn);
        const auto oppRookCandidates = rank & (bbs.rooks(moved) | bbs.queens(moved));

        // not possible :3
        if (!rank[king] || !oppRookCandidates) {
            return;
        }

        const auto pawnlessOcc = bbs.occupancy() ^ squareBit(movedPawn) ^ squareBit(capturingPawn);
        const auto attacks = attacks::getRookAttacks(king, pawnlessOcc);

        if (attacks & oppRookCandidates) {
            unset();
        }
    }

    Move Position::moveFromUci(std::string_view move) const {
        if (move.length() < 4 || move.length() > 5) {
            return kNullMove;
        }

        const auto src = squareFromString(move.substr(0, 2));
        const auto dst = squareFromString(move.substr(2, 2));

        if (move.length() == 5) {
            return Move::promotion(src, dst, pieceTypeFromChar(move[4]));
        } else {
            const auto srcPiece = m_boards.pieceOn(src);

            if (srcPiece == Piece::kBlackKing || srcPiece == Piece::kWhiteKing) {
                if (g_opts.chess960) {
                    if (m_boards.pieceOn(dst) == copyPieceColor(srcPiece, PieceType::kRook)) {
                        return Move::castling(src, dst);
                    } else {
                        return Move::standard(src, dst);
                    }
                } else if (std::abs(squareFile(src) - squareFile(dst)) == 2) {
                    const auto rookFile = squareFile(src) < squareFile(dst) ? 7 : 0;
                    return Move::castling(src, toSquare(squareRank(src), rookFile));
                }
            }

            if ((srcPiece == Piece::kBlackPawn || srcPiece == Piece::kWhitePawn) && dst == m_enPassant) {
                return Move::enPassant(src, dst);
            }

            return Move::standard(src, dst);
        }
    }

    Position Position::starting() {
        Position pos{};

        auto& bbs = pos.m_boards.bbs();

        bbs.forPiece(PieceType::kPawn) = U64(0x00FF00000000FF00);
        bbs.forPiece(PieceType::kKnight) = U64(0x4200000000000042);
        bbs.forPiece(PieceType::kBishop) = U64(0x2400000000000024);
        bbs.forPiece(PieceType::kRook) = U64(0x8100000000000081);
        bbs.forPiece(PieceType::kQueen) = U64(0x0800000000000008);
        bbs.forPiece(PieceType::kKing) = U64(0x1000000000000010);

        bbs.forColor(Color::kBlack) = U64(0xFFFF000000000000);
        bbs.forColor(Color::kWhite) = U64(0x000000000000FFFF);

        pos.m_castlingRooks.black().kingside = Square::kH8;
        pos.m_castlingRooks.black().queenside = Square::kA8;
        pos.m_castlingRooks.white().kingside = Square::kH1;
        pos.m_castlingRooks.white().queenside = Square::kA1;

        pos.m_stm = Color::kWhite;
        pos.m_fullmove = 1;

        pos.regen();

        return pos;
    }

    std::optional<Position> Position::fromFenParts(std::span<const std::string_view> fen) {
        if (fen.size() > 6) {
            eprintln("excess tokens after fullmove number");
            return {};
        }

        if (fen.size() == 5) {
            eprintln("missing fullmove number");
            return {};
        }

        if (fen.size() == 4) {
            eprintln("missing halfmove clock");
            return {};
        }

        if (fen.size() == 3) {
            eprintln("missing en passant square");
            return {};
        }

        if (fen.size() == 2) {
            eprintln("missing castling rights");
            return {};
        }

        if (fen.size() == 1) {
            eprintln("missing side to move");
            return {};
        }

        if (fen.empty()) {
            eprintln("missing ranks");
            return {};
        }

        Position pos{};
        const auto& bbs = pos.bbs();

        u32 rankIdx = 0;

        std::vector<std::string_view> ranks{};
        split::split(ranks, fen[0], '/');

        for (const auto rank : ranks) {
            if (rankIdx >= 8) {
                eprintln("too many ranks");
                return {};
            }

            u32 fileIdx = 0;

            for (const auto c : rank) {
                if (fileIdx >= 8) {
                    eprintln("too many files in rank {}", rankIdx);
                    return {};
                }

                if (const auto emptySquares = util::tryParseDigit(c)) {
                    fileIdx += *emptySquares;
                } else if (const auto piece = pieceFromChar(c); piece != Piece::kNone) {
                    pos.m_boards.setPiece(toSquare(7 - rankIdx, fileIdx), piece);
                    ++fileIdx;
                } else {
                    eprintln("invalid piece character {}", c);
                    return {};
                }
            }

            // last character was a digit
            if (fileIdx > 8) {
                eprintln("too many files in rank {}", rankIdx);
                return {};
            }

            if (fileIdx < 8) {
                eprintln("not enough files in rank {}", rankIdx);
                return {};
            }

            ++rankIdx;
        }

        if (const auto blackKingCount = bbs.forPiece(Piece::kBlackKing).popcount(); blackKingCount != 1) {
            eprintln("black must have exactly 1 king, but has {}", blackKingCount);
            return {};
        }

        if (const auto whiteKingCount = bbs.forPiece(Piece::kWhiteKing).popcount(); whiteKingCount != 1) {
            eprintln("white must have exactly 1 king, but has {}", whiteKingCount);
            return {};
        }

        if (bbs.occupancy().popcount() > 32) {
            eprintln("too many pieces");
            return {};
        }

        const auto color = fen[1];

        if (color.length() != 1) {
            eprintln("invalid side to move");
            return {};
        }

        switch (color[0]) {
            case 'b':
                pos.m_stm = Color::kBlack;
                break;
            case 'w':
                pos.m_stm = Color::kWhite;
                break;
            default:
                eprintln("invalid side to move");
                return {};
        }

        if (const auto stm = pos.stm();
            pos.isAttacked<false>(stm, bbs.forPiece(PieceType::kKing, oppColor(stm)).lowestSquare(), stm))
        {
            eprintln("opponent must not be in check");
            return {};
        }

        const auto castlingRights = fen[2];

        if (castlingRights.length() > 4) {
            eprintln("invalid castling rights");
            return {};
        }

        if (castlingRights != "-") {
            if (g_opts.chess960) {
                for (i32 rank = 0; rank < 8; ++rank) {
                    for (i32 file = 0; file < 8; ++file) {
                        const auto square = toSquare(rank, file);

                        const auto piece = pos.m_boards.pieceOn(square);
                        if (piece != Piece::kNone && pieceType(piece) == PieceType::kKing) {
                            pos.m_kings.color(pieceColor(piece)) = square;
                        }
                    }
                }

                for (const auto flag : castlingRights) {
                    if (flag >= 'a' && flag <= 'h') {
                        const auto file = static_cast<i32>(flag - 'a');
                        const auto kingFile = squareFile(pos.m_kings.black());

                        if (file == kingFile) {
                            eprintln("invalid castling rights");
                            return {};
                        }

                        if (file < kingFile) {
                            pos.m_castlingRooks.black().queenside = toSquare(7, file);
                        } else {
                            pos.m_castlingRooks.black().kingside = toSquare(7, file);
                        }
                    } else if (flag >= 'A' && flag <= 'H') {
                        const auto file = static_cast<i32>(flag - 'A');
                        const auto kingFile = squareFile(pos.m_kings.white());

                        if (file == kingFile) {
                            eprintln("invalid castling rights");
                            return {};
                        }

                        if (file < kingFile) {
                            pos.m_castlingRooks.white().queenside = toSquare(0, file);
                        } else {
                            pos.m_castlingRooks.white().kingside = toSquare(0, file);
                        }
                    } else if (flag == 'k') {
                        for (i32 file = squareFile(pos.m_kings.black()) + 1; file < 8; ++file) {
                            const auto square = toSquare(7, file);
                            if (pos.m_boards.pieceOn(square) == Piece::kBlackRook) {
                                pos.m_castlingRooks.black().kingside = square;
                                break;
                            }
                        }
                    } else if (flag == 'K') {
                        for (i32 file = squareFile(pos.m_kings.white()) + 1; file < 8; ++file) {
                            const auto square = toSquare(0, file);
                            if (pos.m_boards.pieceOn(square) == Piece::kWhiteRook) {
                                pos.m_castlingRooks.white().kingside = square;
                                break;
                            }
                        }
                    } else if (flag == 'q') {
                        for (i32 file = squareFile(pos.m_kings.black()) - 1; file >= 0; --file) {
                            const auto square = toSquare(7, file);
                            if (pos.m_boards.pieceOn(square) == Piece::kBlackRook) {
                                pos.m_castlingRooks.black().queenside = square;
                                break;
                            }
                        }
                    } else if (flag == 'Q') {
                        for (i32 file = squareFile(pos.m_kings.white()) - 1; file >= 0; --file) {
                            const auto square = toSquare(0, file);
                            if (pos.m_boards.pieceOn(square) == Piece::kWhiteRook) {
                                pos.m_castlingRooks.white().queenside = square;
                                break;
                            }
                        }
                    } else {
                        eprintln("invalid castling rights");
                        return {};
                    }
                }
            } else {
                for (const auto flag : castlingRights) {
                    switch (flag) {
                        case 'k':
                            pos.m_castlingRooks.black().kingside = Square::kH8;
                            break;
                        case 'q':
                            pos.m_castlingRooks.black().queenside = Square::kA8;
                            break;
                        case 'K':
                            pos.m_castlingRooks.white().kingside = Square::kH1;
                            break;
                        case 'Q':
                            pos.m_castlingRooks.white().queenside = Square::kA1;
                            break;
                        default:
                            eprintln("invalid castling rights");
                            return {};
                    }
                }
            }
        }

        const auto enPassant = fen[3];

        if (enPassant != "-") {
            if (pos.m_enPassant = squareFromString(enPassant); pos.m_enPassant == Square::kNone) {
                eprintln("invalid en passant square");
                return {};
            }
        }

        const auto halfmove = fen[4];
        if (!util::tryParse(pos.m_halfmove, halfmove)) {
            eprintln("invalid halfmove clock");
            return {};
        }

        const auto fullmove = fen[5];
        if (!util::tryParse(pos.m_fullmove, fullmove)) {
            eprintln("invalid fullmove number");
            return {};
        }

        pos.regen();

        return pos;
    }

    std::optional<Position> Position::fromFen(std::string_view fen) {
        std::vector<std::string_view> parts{};
        parts.reserve(6);

        split::split(parts, fen, ' ');

        return fromFenParts(parts);
    }

    std::optional<Position> Position::fromFrcIndex(u32 n) {
        assert(g_opts.chess960);

        if (n >= 960) {
            eprintln("invalid frc position index {}", n);
            return {};
        }

        Position pos{};

        auto& bbs = pos.m_boards.bbs();

        bbs.forPiece(PieceType::kPawn) = U64(0x00FF00000000FF00);

        bbs.forColor(Color::kBlack) = U64(0x00FF000000000000);
        bbs.forColor(Color::kWhite) = U64(0x000000000000FF00);

        const auto backrank = scharnaglToBackrank(n);

        bool firstRook = true;

        for (i32 i = 0; i < 8; ++i) {
            const auto blackSquare = toSquare(7, i);
            const auto whiteSquare = toSquare(0, i);

            pos.m_boards.setPiece(blackSquare, colorPiece(backrank[i], Color::kBlack));
            pos.m_boards.setPiece(whiteSquare, colorPiece(backrank[i], Color::kWhite));

            if (backrank[i] == PieceType::kRook) {
                if (firstRook) {
                    pos.m_castlingRooks.black().queenside = blackSquare;
                    pos.m_castlingRooks.white().queenside = whiteSquare;
                } else {
                    pos.m_castlingRooks.black().kingside = blackSquare;
                    pos.m_castlingRooks.white().kingside = whiteSquare;
                }

                firstRook = false;
            }
        }

        pos.m_stm = Color::kWhite;
        pos.m_fullmove = 1;

        pos.regen();

        return pos;
    }

    std::optional<Position> Position::fromDfrcIndex(u32 n) {
        assert(g_opts.chess960);

        if (n >= 960 * 960) {
            eprintln("invalid dfrc position index {}", n);
            return {};
        }

        Position pos{};

        auto& bbs = pos.m_boards.bbs();

        bbs.forPiece(PieceType::kPawn) = U64(0x00FF00000000FF00);

        bbs.forColor(Color::kBlack) = U64(0x00FF000000000000);
        bbs.forColor(Color::kWhite) = U64(0x000000000000FF00);

        const auto blackBackrank = scharnaglToBackrank(n / 960);
        const auto whiteBackrank = scharnaglToBackrank(n % 960);

        bool firstBlackRook = true;
        bool firstWhiteRook = true;

        for (i32 i = 0; i < 8; ++i) {
            const auto blackSquare = toSquare(7, i);
            const auto whiteSquare = toSquare(0, i);

            pos.m_boards.setPiece(blackSquare, colorPiece(blackBackrank[i], Color::kBlack));
            pos.m_boards.setPiece(whiteSquare, colorPiece(whiteBackrank[i], Color::kWhite));

            if (blackBackrank[i] == PieceType::kRook) {
                if (firstBlackRook) {
                    pos.m_castlingRooks.black().queenside = blackSquare;
                } else {
                    pos.m_castlingRooks.black().kingside = blackSquare;
                }

                firstBlackRook = false;
            }

            if (whiteBackrank[i] == PieceType::kRook) {
                if (firstWhiteRook) {
                    pos.m_castlingRooks.white().queenside = whiteSquare;
                } else {
                    pos.m_castlingRooks.white().kingside = whiteSquare;
                }

                firstWhiteRook = false;
            }
        }

        pos.m_stm = Color::kWhite;
        pos.m_fullmove = 1;

        pos.regen();

        return pos;
    }

    Square squareFromString(std::string_view str) {
        if (str.length() != 2) {
            return Square::kNone;
        }

        const auto file = str[0];
        const auto rank = str[1];

        if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
            return Square::kNone;
        }

        return toSquare(static_cast<u32>(rank - '1'), static_cast<u32>(file - 'a'));
    }
} // namespace stormphrax

fmt::format_context::iterator fmt::formatter<stormphrax::Position>::format(
    const stormphrax::Position& value,
    format_context& ctx
) const {
    using namespace stormphrax;

    const auto& boards = value.boards();

    for (i32 rank = 7; rank >= 0; --rank) {
        format_to(ctx.out(), " +---+---+---+---+---+---+---+---+\n");

        for (i32 file = 0; file < 8; ++file) {
            const auto piece = boards.pieceAt(rank, file);
            format_to(ctx.out(), " | {}", piece);
        }

        format_to(ctx.out(), " | {}\n", rank + 1);
    }

    format_to(ctx.out(), " +---+---+---+---+---+---+---+---+\n");
    format_to(ctx.out(), "   a   b   c   d   e   f   g   h\n");

    format_to(ctx.out(), "\n");

    format_to(ctx.out(), "{} to move", value.stm() == Color::kBlack ? "Black" : "White");

    return ctx.out();
}
