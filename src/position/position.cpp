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
            static constexpr std::array kN5n = {
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

            const auto placeInNthFree = [&dst](u32 n, PieceType pt) {
                for (i32 free = 0, i = 0; i < 8; ++i) {
                    if (dst[i] == PieceTypes::kPawn && free++ == n) {
                        dst[i] = pt;
                        break;
                    }
                }
            };

            const auto placeInFirstFree = [&dst](PieceType pt) {
                for (i32 i = 0; i < 8; ++i) {
                    if (dst[i] == PieceTypes::kPawn) {
                        dst[i] = pt;
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

            dst[b1 * 2 + 1] = PieceTypes::kBishop;
            dst[b2 * 2] = PieceTypes::kBishop;

            placeInNthFree(q, PieceTypes::kQueen);

            const auto [knight1, knight2] = kN5n[n4];

            placeInNthFree(knight1, PieceTypes::kKnight);
            placeInNthFree(knight2, PieceTypes::kKnight);

            placeInFirstFree(PieceTypes::kRook);
            placeInFirstFree(PieceTypes::kKing);
            placeInFirstFree(PieceTypes::kRook);

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

        newPos.m_stm = m_stm.flip();
        newPos.m_keys.flipStm();

        if (newPos.m_enPassant != Squares::kNone) {
            newPos.m_keys.flipEp(newPos.m_enPassant);
            newPos.m_enPassant = Squares::kNone;
        }

        const auto stm = newPos.nstm();
        const auto nstm = stm.flip();

        if (stm == Colors::kBlack) {
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
        const auto movingType = moving.type();

        eval::NnueUpdates updates{};
        auto captured = Pieces::kNone;

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

        assert(captured.typeOrNone() != PieceTypes::kKing);

        if constexpr (kUpdateNnue) {
            nnueState->pushUpdates<kNnueAction == NnueUpdateAction::kApply>(updates, m_boards.bbs(), m_kings);
        }

        if (movingType == PieceTypes::kRook) {
            newPos.m_castlingRooks.color(stm).unset(moveSrc);
        } else if (movingType == PieceTypes::kKing) {
            newPos.m_castlingRooks.color(stm).clear();
        } else if (movingType == PieceTypes::kPawn && std::abs(move.fromSqRank() - move.toSqRank()) == 2) {
            newPos.m_enPassant = move.toSq().flipRankParity();
            newPos.m_keys.flipEp(newPos.m_enPassant);
        }

        if (captured == Pieces::kNone && moving.type() != PieceTypes::kPawn) {
            ++newPos.m_halfmove;
        } else {
            newPos.m_halfmove = 0;
        }

        if (captured != Pieces::kNone && captured.type() == PieceTypes::kRook) {
            newPos.m_castlingRooks.color(nstm).unset(moveDst);
        }

        if (newPos.m_castlingRooks != m_castlingRooks) {
            newPos.m_keys.switchCastling(m_castlingRooks, newPos.m_castlingRooks);
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

        if (srcPiece == Pieces::kNone || srcPiece.color() != us) {
            return false;
        }

        const auto type = move.type();

        const auto dst = move.toSq();
        const auto dstPiece = m_boards.pieceOn(dst);

        // we're capturing something
        if (dstPiece != Pieces::kNone
            // we're capturing our own piece
            && ((dstPiece.color() == us
                 //  and either not castling
                 && (type != MoveType::kCastling
                     // or trying to castle with a non-rook
                     || dstPiece != PieceTypes::kRook.withColor(us)))
                // or trying to capture a king
                || dstPiece.type() == PieceTypes::kKing))
        {
            return false;
        }

        const auto srcPieceType = srcPiece.type();
        const auto them = us.flip();
        const auto occ = m_boards.bbs().occupancy();

        if (type == MoveType::kCastling) {
            if (srcPieceType != PieceTypes::kKing || isCheck()) {
                return false;
            }

            const auto homeRank = relativeRank(us, 0);

            // wrong rank
            if (move.fromSqRank() != homeRank || move.toSqRank() != homeRank) {
                return false;
            }

            Square kingDst, rookDst;

            if (src.file() < dst.file()) {
                // no castling rights
                if (dst != m_castlingRooks.color(us).kingside) {
                    return false;
                }

                kingDst = src.withFile(kFileG);
                rookDst = src.withFile(kFileF);
            } else {
                // no castling rights
                if (dst != m_castlingRooks.color(us).queenside) {
                    return false;
                }

                kingDst = src.withFile(kFileC);
                rookDst = src.withFile(kFileD);
            }

            // same checks as for movegen
            if (g_opts.chess960) {
                const auto toKingDst = rayBetween(src, kingDst);
                const auto toRook = rayBetween(src, dst);

                const auto castleOcc = occ ^ src.bit() ^ dst.bit();

                return (castleOcc & (toKingDst | toRook | kingDst.bit() | rookDst.bit())).empty()
                    && !anyAttacked(toKingDst | kingDst.bit(), them);
            } else {
                if (dst == m_castlingRooks.black().kingside) {
                    return (occ & U64(0x6000000000000000)).empty() && !isAttacked(Squares::kF8, Colors::kWhite);
                } else if (dst == m_castlingRooks.black().queenside) {
                    return (occ & U64(0x0E00000000000000)).empty() && !isAttacked(Squares::kD8, Colors::kWhite);
                } else if (dst == m_castlingRooks.white().kingside) {
                    return (occ & U64(0x0000000000000060)).empty() && !isAttacked(Squares::kF1, Colors::kBlack);
                } else {
                    return (occ & U64(0x000000000000000E)).empty() && !isAttacked(Squares::kD1, Colors::kBlack);
                }
            }
        }

        if (srcPieceType == PieceTypes::kPawn) {
            if (type == MoveType::kEnPassant) {
                return dst == m_enPassant && attacks::getPawnAttacks(m_enPassant, them)[src];
            }

            const auto srcRank = move.fromSqRank();
            const auto dstRank = move.toSqRank();

            // backwards move
            if ((us == Colors::kBlack && dstRank >= srcRank) || (us == Colors::kWhite && dstRank <= srcRank)) {
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
            } else if (dstPiece != Pieces::kNone) {
                // forward move onto a piece
                return false;
            }

            const auto delta = std::abs(dstRank - srcRank);
            const auto maxDelta = 1 + (srcRank == relativeRank(us, kRank2));

            if (delta > maxDelta) {
                return false;
            }

            if (delta == 2 && occ[dst.flipRankParity()]) {
                return false;
            }
        } else {
            if (type == MoveType::kPromotion || type == MoveType::kEnPassant) {
                return false;
            }

            Bitboard attacks{};

            switch (srcPieceType.raw()) {
                case PieceTypes::kKnight.raw():
                    attacks = attacks::getKnightAttacks(src);
                    break;
                case PieceTypes::kBishop.raw():
                    attacks = attacks::getBishopAttacks(src, occ);
                    break;
                case PieceTypes::kRook.raw():
                    attacks = attacks::getRookAttacks(src, occ);
                    break;
                case PieceTypes::kQueen.raw():
                    attacks = attacks::getQueenAttacks(src, occ);
                    break;
                case PieceTypes::kKing.raw():
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
        const auto them = us.flip();

        const auto& bbs = m_boards.bbs();

        const auto src = move.fromSq();
        const auto dst = move.toSq();

        const auto king = m_kings.color(us);

        if (move.type() == MoveType::kCastling) {
            const auto kingDst = move.fromSq().withFile(move.fromSqFile() < move.toSqFile() ? kFileG : kFileC);
            return !m_threats[kingDst] && !(g_opts.chess960 && pinned(us)[dst]);
        } else if (move.type() == MoveType::kEnPassant) {
            const auto captureSquare = dst.flipRankParity();
            const auto postEpOcc = bbs.occupancy() ^ Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst)
                                 ^ Bitboard::fromSquare(captureSquare);

            const auto theirQueens = bbs.queens(them);

            return (attacks::getBishopAttacks(king, postEpOcc) & (theirQueens | bbs.bishops(them))).empty()
                && (attacks::getRookAttacks(king, postEpOcc) & (theirQueens | bbs.rooks(them))).empty();
        }

        const auto moving = m_boards.pieceOn(src);

        if (moving.type() == PieceTypes::kKing) {
            const auto kinglessOcc = bbs.occupancy() ^ bbs.kings(us);
            const auto theirQueens = bbs.queens(them);

            return !m_threats[move.toSq()]
                && (attacks::getBishopAttacks(dst, kinglessOcc) & (theirQueens | bbs.bishops(them))).empty()
                && (attacks::getRookAttacks(dst, kinglessOcc) & (theirQueens | bbs.rooks(them))).empty();
        }

        // multiple checks can only be evaded with a king move
        if (m_checkers.multiple()) {
            return false;
        }

        if (pinned(us)[src] && !rayIntersecting(src, dst)[king]) {
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
                if (piece == Pieces::kNone) {
                    piece = m_boards.pieceOn(move.toSq());
                }

                assert(piece != Pieces::kNone);

                return piece.color() == stm();
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
                if (m_boards.pieceOn(rank, file) == Pieces::kNone) {
                    u32 emptySquares = 1;
                    for (; file < 7 && m_boards.pieceOn(rank, file + 1) == Pieces::kNone; ++file, ++emptySquares) {}

                    fmt::format_to(itr, "{}", static_cast<char>('0' + emptySquares));
                } else {
                    fmt::format_to(itr, "{}", m_boards.pieceOn(rank, file));
                }
            }

            if (rank > 0) {
                fmt::format_to(itr, "/");
            }
        }

        fmt::format_to(itr, "{}", stm() == Colors::kWhite ? " w " : " b ");

        if (m_castlingRooks == CastlingRooks{}) {
            fmt::format_to(itr, "-");
        } else if (g_opts.chess960) {
            if (m_castlingRooks.white().kingside != Squares::kNone) {
                fmt::format_to(itr, "{}", static_cast<char>('A' + m_castlingRooks.white().kingside.file()));
            }

            if (m_castlingRooks.white().queenside != Squares::kNone) {
                fmt::format_to(itr, "{}", static_cast<char>('A' + m_castlingRooks.white().queenside.file()));
            }

            if (m_castlingRooks.black().kingside != Squares::kNone) {
                fmt::format_to(itr, "{}", static_cast<char>('a' + m_castlingRooks.black().kingside.file()));
            }

            if (m_castlingRooks.black().queenside != Squares::kNone) {
                fmt::format_to(itr, "{}", static_cast<char>('a' + m_castlingRooks.black().queenside.file()));
            }
        } else {
            if (m_castlingRooks.white().kingside != Squares::kNone) {
                fmt::format_to(itr, "K");
            }

            if (m_castlingRooks.white().queenside != Squares::kNone) {
                fmt::format_to(itr, "Q");
            }

            if (m_castlingRooks.black().kingside != Squares::kNone) {
                fmt::format_to(itr, "k");
            }

            if (m_castlingRooks.black().queenside != Squares::kNone) {
                fmt::format_to(itr, "q");
            }
        }

        if (m_enPassant != Squares::kNone) {
            fmt::format_to(itr, " {}", m_enPassant);
        } else {
            fmt::format_to(itr, " -");
        }

        fmt::format_to(itr, " {} {}", m_halfmove, m_fullmove);

        return fen;
    }

    template <bool kUpdateKey>
    void Position::setPiece(Piece piece, Square sq) {
        assert(piece != Pieces::kNone);
        assert(sq != Squares::kNone);

        assert(piece.type() != PieceTypes::kKing);

        m_boards.setPiece(sq, piece);

        if constexpr (kUpdateKey) {
            m_keys.flipPiece(piece, sq);
        }
    }

    template <bool kUpdateKey>
    void Position::removePiece(Piece piece, Square sq) {
        assert(piece != Pieces::kNone);
        assert(sq != Squares::kNone);

        assert(piece.type() != PieceTypes::kKing);

        m_boards.removePiece(sq, piece);

        if constexpr (kUpdateKey) {
            m_keys.flipPiece(piece, sq);
        }
    }

    template <bool kUpdateKey>
    void Position::movePieceNoCap(Piece piece, Square src, Square dst) {
        assert(piece != Pieces::kNone);

        assert(src != Squares::kNone);
        assert(dst != Squares::kNone);

        if (src == dst) {
            return;
        }

        m_boards.movePiece(src, dst, piece);

        if (piece.type() == PieceTypes::kKing) {
            const auto color = piece.color();
            m_kings.color(color) = dst;
        }

        if constexpr (kUpdateKey) {
            m_keys.movePiece(piece, src, dst);
        }
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    Piece Position::movePiece(Piece piece, Square src, Square dst, eval::NnueUpdates& nnueUpdates) {
        assert(piece != Pieces::kNone);

        assert(src != Squares::kNone);
        assert(dst != Squares::kNone);
        assert(src != dst);

        const auto captured = m_boards.pieceOn(dst);

        if (captured != Pieces::kNone) {
            assert(captured.type() != PieceTypes::kKing);

            m_boards.removePiece(dst, captured);

            // NNUE update done below

            if constexpr (kUpdateKey) {
                m_keys.flipPiece(captured, dst);
            }
        }

        m_boards.movePiece(src, dst, piece);

        if (piece.type() == PieceTypes::kKing) {
            const auto color = piece.color();

            if constexpr (kUpdateNnue) {
                if (eval::InputFeatureSet::refreshRequired(color, m_kings.color(color), dst)) {
                    nnueUpdates.setRefresh(color);
                }
            }

            m_kings.color(color) = dst;
        }

        if constexpr (kUpdateNnue) {
            nnueUpdates.pushSubAdd(piece, src, dst);

            if (captured != Pieces::kNone) {
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
        assert(pawn != Pieces::kNone);
        assert(pawn.type() == PieceTypes::kPawn);

        assert(src != Squares::kNone);
        assert(dst != Squares::kNone);
        assert(src != dst);

        assert(dst.rank() == relativeRank(pawn.color(), 7));
        assert(src.rank() == relativeRank(pawn.color(), 6));

        assert(promo != PieceTypes::kNone);

        const auto captured = m_boards.pieceOn(dst);

        if (captured != Pieces::kNone) {
            assert(captured.type() != PieceTypes::kKing);

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
            const auto coloredPromo = pawn.copyColor(promo);

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
        assert(king != Pieces::kNone);
        assert(king.type() == PieceTypes::kKing);

        assert(kingSrc != Squares::kNone);
        assert(rookSrc != Squares::kNone);
        assert(kingSrc != rookSrc);

        Square kingDst, rookDst;

        if (kingSrc.file() < rookSrc.file()) {
            // short
            kingDst = kingSrc.withFile(kFileG);
            rookDst = kingSrc.withFile(kFileF);
        } else {
            // long
            kingDst = kingSrc.withFile(kFileC);
            rookDst = kingSrc.withFile(kFileD);
        }

        const auto rook = king.copyColor(PieceTypes::kRook);

        movePieceNoCap<kUpdateKey>(king, kingSrc, kingDst);
        movePieceNoCap<kUpdateKey>(rook, rookSrc, rookDst);

        if constexpr (kUpdateNnue) {
            const auto color = king.color();

            if (eval::InputFeatureSet::refreshRequired(color, kingSrc, kingDst)) {
                nnueUpdates.setRefresh(color);
            }

            nnueUpdates.pushSubAdd(king, kingSrc, kingDst);
            nnueUpdates.pushSubAdd(rook, rookSrc, rookDst);
        }
    }

    template <bool kUpdateKey, bool kUpdateNnue>
    Piece Position::enPassant(Piece pawn, Square src, Square dst, eval::NnueUpdates& nnueUpdates) {
        assert(pawn != Pieces::kNone);
        assert(pawn.type() == PieceTypes::kPawn);

        assert(src != Squares::kNone);
        assert(dst != Squares::kNone);
        assert(src != dst);

        m_boards.movePiece(src, dst, pawn);

        if constexpr (kUpdateNnue) {
            nnueUpdates.pushSubAdd(pawn, src, dst);
        }

        if constexpr (kUpdateKey) {
            m_keys.movePiece(pawn, src, dst);
        }

        const auto captureSquare = dst.flipRankParity();
        const auto enemyPawn = pawn.flipColor();

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

        for (i32 rank = 0; rank < 8; ++rank) {
            for (i32 file = 0; file < 8; ++file) {
                const auto sq = Square::fromFileRank(file, rank);
                if (const auto piece = m_boards.pieceOn(sq); piece != Pieces::kNone) {
                    if (piece.type() == PieceTypes::kKing) {
                        m_kings.color(piece.color()) = sq;
                    }

                    m_keys.flipPiece(piece, Square::fromFileRank(file, rank));
                }
            }
        }

        m_keys.flipCastling(m_castlingRooks);
        m_keys.flipEp(m_enPassant);

        if (stm() == Colors::kBlack) {
            m_keys.flipStm();
        }

        m_checkers = calcCheckers();
        m_pinned = calcPinned();
        m_threats = calcThreats();

        filterEp(stm());
    }

    void Position::filterEp(Color capturing) {
        if (m_enPassant == Squares::kNone) {
            return;
        }

        const auto unset = [this] {
            m_keys.flipEp(m_enPassant);
            m_enPassant = Squares::kNone;
        };

        const auto& bbs = m_boards.bbs();

        const auto moved = capturing.flip();

        const auto king = m_kings.color(capturing);

        const auto pinnedPieces = pinned(capturing);
        auto candidates = bbs.pawns(capturing) & attacks::getPawnAttacks(m_enPassant, moved);

        // vertically pinned pawns cannot capture at all
        const auto vertPinned = pinnedPieces & boards::kFiles[king.file()];
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
        const auto movedPawn = m_enPassant.flipRankParity();
        const auto capturingPawn = candidates.lowestSquare();

        const auto rank = rayIntersecting(movedPawn, capturingPawn);
        const auto oppRookCandidates = rank & (bbs.rooks(moved) | bbs.queens(moved));

        // not possible :3
        if (!rank[king] || !oppRookCandidates) {
            return;
        }

        const auto pawnlessOcc = bbs.occupancy() ^ movedPawn.bit() ^ capturingPawn.bit();
        const auto attacks = attacks::getRookAttacks(king, pawnlessOcc);

        if (attacks & oppRookCandidates) {
            unset();
        }
    }

    Move Position::moveFromUci(std::string_view move) const {
        if (move.length() < 4 || move.length() > 5) {
            return kNullMove;
        }

        const auto src = Square::fromStr(move.substr(0, 2));
        const auto dst = Square::fromStr(move.substr(2, 2));

        if (move.length() == 5) {
            const auto promo = PieceType::fromChar(move[4]);

            if (!promo.isValidPromotion()) {
                return kNullMove;
            }

            return Move::promotion(src, dst, promo);
        } else {
            const auto srcPiece = m_boards.pieceOn(src);

            if (srcPiece == Pieces::kBlackKing || srcPiece == Pieces::kWhiteKing) {
                if (g_opts.chess960) {
                    if (m_boards.pieceOn(dst) == srcPiece.copyColor(PieceTypes::kRook)) {
                        return Move::castling(src, dst);
                    } else {
                        return Move::standard(src, dst);
                    }
                } else if (std::abs(src.file() - dst.file()) == 2) {
                    const auto rookFile = src.file() < dst.file() ? 7 : 0;
                    return Move::castling(src, Square::fromFileRank(rookFile, src.rank()));
                }
            }

            if ((srcPiece == Pieces::kBlackPawn || srcPiece == Pieces::kWhitePawn) && dst == m_enPassant) {
                return Move::enPassant(src, dst);
            }

            return Move::standard(src, dst);
        }
    }

    Position Position::starting() {
        Position pos{};

        auto& bbs = pos.m_boards.bbs();

        bbs.forPiece(PieceTypes::kPawn) = U64(0x00FF00000000FF00);
        bbs.forPiece(PieceTypes::kKnight) = U64(0x4200000000000042);
        bbs.forPiece(PieceTypes::kBishop) = U64(0x2400000000000024);
        bbs.forPiece(PieceTypes::kRook) = U64(0x8100000000000081);
        bbs.forPiece(PieceTypes::kQueen) = U64(0x0800000000000008);
        bbs.forPiece(PieceTypes::kKing) = U64(0x1000000000000010);

        bbs.forColor(Colors::kBlack) = U64(0xFFFF000000000000);
        bbs.forColor(Colors::kWhite) = U64(0x000000000000FFFF);

        pos.m_castlingRooks.black().kingside = Squares::kH8;
        pos.m_castlingRooks.black().queenside = Squares::kA8;
        pos.m_castlingRooks.white().kingside = Squares::kH1;
        pos.m_castlingRooks.white().queenside = Squares::kA1;

        pos.m_stm = Colors::kWhite;
        pos.m_fullmove = 1;

        pos.regen();

        return pos;
    }

    std::optional<Position> Position::fromFenParts(std::span<const std::string_view> fen) {
        if (fen.size() < 4 || fen.size() > 6) {
            eprintln("wrong number of FEN parts");
            return {};
        }

        Position pos{};
        const auto& bbs = pos.bbs();

        i32 rankIdx = 0;

        std::vector<std::string_view> ranks{};
        split::split(ranks, fen[0], '/');

        for (const auto rank : ranks) {
            if (rankIdx >= 8) {
                eprintln("too many ranks");
                return {};
            }

            i32 fileIdx = 0;

            for (const auto c : rank) {
                if (fileIdx >= 8) {
                    eprintln("too many files in rank {}", rankIdx);
                    return {};
                }

                if (const auto emptySquares = util::tryParseDigit(c)) {
                    fileIdx += *emptySquares;
                } else if (const auto piece = Piece::fromChar(c); piece != Pieces::kNone) {
                    pos.m_boards.setPiece(Square::fromFileRank(fileIdx, 7 - rankIdx), piece);
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

        if (const auto blackKingCount = bbs.forPiece(Pieces::kBlackKing).popcount(); blackKingCount != 1) {
            eprintln("black must have exactly 1 king, but has {}", blackKingCount);
            return {};
        }

        if (const auto whiteKingCount = bbs.forPiece(Pieces::kWhiteKing).popcount(); whiteKingCount != 1) {
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
                pos.m_stm = Colors::kBlack;
                break;
            case 'w':
                pos.m_stm = Colors::kWhite;
                break;
            default:
                eprintln("invalid side to move");
                return {};
        }

        if (const auto stm = pos.stm();
            pos.isAttacked<false>(stm, bbs.forPiece(PieceTypes::kKing, stm.flip()).lowestSquare(), stm))
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
                        const auto sq = Square::fromFileRank(file, rank);

                        const auto piece = pos.m_boards.pieceOn(sq);
                        if (piece != Pieces::kNone && piece.type() == PieceTypes::kKing) {
                            pos.m_kings.color(piece.color()) = sq;
                        }
                    }
                }

                for (const auto flag : castlingRights) {
                    if (flag >= 'a' && flag <= 'h') {
                        const auto file = static_cast<i32>(flag - 'a');
                        const auto kingFile = pos.m_kings.black().file();

                        if (file == kingFile) {
                            eprintln("invalid castling rights");
                            return {};
                        }

                        if (file < kingFile) {
                            pos.m_castlingRooks.black().queenside = Square::fromFileRank(file, 7);
                        } else {
                            pos.m_castlingRooks.black().kingside = Square::fromFileRank(file, 7);
                        }
                    } else if (flag >= 'A' && flag <= 'H') {
                        const auto file = static_cast<i32>(flag - 'A');
                        const auto kingFile = pos.m_kings.white().file();

                        if (file == kingFile) {
                            eprintln("invalid castling rights");
                            return {};
                        }

                        if (file < kingFile) {
                            pos.m_castlingRooks.white().queenside = Square::fromFileRank(file, 0);
                        } else {
                            pos.m_castlingRooks.white().kingside = Square::fromFileRank(file, 0);
                        }
                    } else if (flag == 'k') {
                        for (i32 file = pos.m_kings.black().file() + 1; file < 8; ++file) {
                            const auto sq = Square::fromFileRank(file, 7);
                            if (pos.m_boards.pieceOn(sq) == Pieces::kBlackRook) {
                                pos.m_castlingRooks.black().kingside = sq;
                                break;
                            }
                        }
                    } else if (flag == 'K') {
                        for (i32 file = pos.m_kings.white().file() + 1; file < 8; ++file) {
                            const auto sq = Square::fromFileRank(file, 0);
                            if (pos.m_boards.pieceOn(sq) == Pieces::kWhiteRook) {
                                pos.m_castlingRooks.white().kingside = sq;
                                break;
                            }
                        }
                    } else if (flag == 'q') {
                        for (i32 file = pos.m_kings.black().file() - 1; file >= 0; --file) {
                            const auto sq = Square::fromFileRank(file, 7);
                            if (pos.m_boards.pieceOn(sq) == Pieces::kBlackRook) {
                                pos.m_castlingRooks.black().queenside = sq;
                                break;
                            }
                        }
                    } else if (flag == 'Q') {
                        for (i32 file = pos.m_kings.white().file() - 1; file >= 0; --file) {
                            const auto sq = Square::fromFileRank(file, 0);
                            if (pos.m_boards.pieceOn(sq) == Pieces::kWhiteRook) {
                                pos.m_castlingRooks.white().queenside = sq;
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
                            pos.m_castlingRooks.black().kingside = Squares::kH8;
                            break;
                        case 'q':
                            pos.m_castlingRooks.black().queenside = Squares::kA8;
                            break;
                        case 'K':
                            pos.m_castlingRooks.white().kingside = Squares::kH1;
                            break;
                        case 'Q':
                            pos.m_castlingRooks.white().queenside = Squares::kA1;
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
            if (pos.m_enPassant = Square::fromStr(enPassant); pos.m_enPassant == Squares::kNone) {
                eprintln("invalid en passant square");
                return {};
            }
        }

        if (fen.size() >= 5) {
            const auto halfmove = fen[4];
            if (!util::tryParse(pos.m_halfmove, halfmove)) {
                eprintln("invalid halfmove clock");
                return {};
            }
        }

        if (fen.size() >= 6) {
            const auto fullmove = fen[5];
            if (!util::tryParse(pos.m_fullmove, fullmove)) {
                eprintln("invalid fullmove number");
                return {};
            }
        }

        // a couple of extra checks here
        if (pos.m_enPassant != Squares::kNone) {
            [&] {
                const auto epRank = pos.m_stm == Colors::kBlack ? kRank3 : kRank6;
                if (pos.m_enPassant.rank() != epRank) {
                    pos.m_enPassant = Squares::kNone;
                    return;
                }

                const auto pawnSquare = pos.m_enPassant.flipRankParity();
                const auto origSquare = pawnSquare.flipDoublePush();

                const auto oppPawn = PieceTypes::kPawn.withColor(pos.m_stm.flip());

                // make sure that there's actually a pawn there that could've moved
                if (pos.m_boards.pieceOn(pawnSquare) != oppPawn               //
                    || pos.m_boards.pieceOn(pos.m_enPassant) != Pieces::kNone //
                    || pos.m_boards.pieceOn(origSquare) != Pieces::kNone)
                {
                    pos.m_enPassant = Squares::kNone;
                    return;
                }

                // and ensure that the previous position would've actually
                // been legal if the previous move was a double push
                pos.m_boards.movePiece(pawnSquare, origSquare, oppPawn);
                const bool illegal = pos.isAttacked<false>(
                    pos.m_stm.flip(),
                    bbs.forPiece(PieceTypes::kKing, pos.m_stm).lowestSquare(),
                    pos.m_stm.flip()
                );
                pos.m_boards.movePiece(origSquare, pawnSquare, oppPawn);

                if (illegal) {
                    pos.m_enPassant = Squares::kNone;
                    return;
                }
            }();
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

        bbs.forPiece(PieceTypes::kPawn) = U64(0x00FF00000000FF00);

        bbs.forColor(Colors::kBlack) = U64(0x00FF000000000000);
        bbs.forColor(Colors::kWhite) = U64(0x000000000000FF00);

        const auto backrank = scharnaglToBackrank(n);

        bool firstRook = true;

        for (i32 i = 0; i < 8; ++i) {
            const auto blackSquare = Square::fromFileRank(i, 7);
            const auto whiteSquare = Square::fromFileRank(i, 0);

            pos.m_boards.setPiece(blackSquare, backrank[i].withColor(Colors::kBlack));
            pos.m_boards.setPiece(whiteSquare, backrank[i].withColor(Colors::kWhite));

            if (backrank[i] == PieceTypes::kRook) {
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

        pos.m_stm = Colors::kWhite;
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

        bbs.forPiece(PieceTypes::kPawn) = U64(0x00FF00000000FF00);

        bbs.forColor(Colors::kBlack) = U64(0x00FF000000000000);
        bbs.forColor(Colors::kWhite) = U64(0x000000000000FF00);

        const auto blackBackrank = scharnaglToBackrank(n / 960);
        const auto whiteBackrank = scharnaglToBackrank(n % 960);

        bool firstBlackRook = true;
        bool firstWhiteRook = true;

        for (i32 i = 0; i < 8; ++i) {
            const auto blackSquare = Square::fromFileRank(i, 7);
            const auto whiteSquare = Square::fromFileRank(i, 0);

            pos.m_boards.setPiece(blackSquare, blackBackrank[i].withColor(Colors::kBlack));
            pos.m_boards.setPiece(whiteSquare, whiteBackrank[i].withColor(Colors::kWhite));

            if (blackBackrank[i] == PieceTypes::kRook) {
                if (firstBlackRook) {
                    pos.m_castlingRooks.black().queenside = blackSquare;
                } else {
                    pos.m_castlingRooks.black().kingside = blackSquare;
                }

                firstBlackRook = false;
            }

            if (whiteBackrank[i] == PieceTypes::kRook) {
                if (firstWhiteRook) {
                    pos.m_castlingRooks.white().queenside = whiteSquare;
                } else {
                    pos.m_castlingRooks.white().kingside = whiteSquare;
                }

                firstWhiteRook = false;
            }
        }

        pos.m_stm = Colors::kWhite;
        pos.m_fullmove = 1;

        pos.regen();

        return pos;
    }
} // namespace stormphrax

fmt::format_context::iterator fmt::formatter<stormphrax::Position>::format(
    const stormphrax::Position& value,
    format_context& ctx
) const {
    using namespace stormphrax;

    const auto& boards = value.boards();

    for (i32 rank = kRank8; rank >= kRank1; --rank) {
        format_to(ctx.out(), " +---+---+---+---+---+---+---+---+\n");

        for (i32 file = kFileA; file <= kFileH; ++file) {
            const auto piece = boards.pieceOn(rank, file);
            format_to(ctx.out(), " | {}", piece);
        }

        format_to(ctx.out(), " | {}\n", rank + 1);
    }

    format_to(ctx.out(), " +---+---+---+---+---+---+---+---+\n");
    format_to(ctx.out(), "   a   b   c   d   e   f   g   h\n");

    format_to(ctx.out(), "\n");

    format_to(ctx.out(), "{} to move", value.stm() == Colors::kBlack ? "Black" : "White");

    return ctx.out();
}
