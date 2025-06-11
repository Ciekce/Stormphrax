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

#include "movegen.h"

#include <algorithm>
#include <array>

#include "attacks/attacks.h"
#include "opts.h"
#include "rays.h"
#include "util/bitfield.h"

namespace stormphrax {
    namespace {
        inline void pushStandards(ScoredMoveList& dst, i32 offset, Bitboard board) {
            while (!board.empty()) {
                const auto dstSquare = board.popLowestSquare();
                const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

                dst.push({Move::standard(srcSquare, dstSquare), 0});
            }
        }

        inline void pushStandards(ScoredMoveList& dst, Square srcSquare, Bitboard board) {
            while (!board.empty()) {
                const auto dstSquare = board.popLowestSquare();
                dst.push({Move::standard(srcSquare, dstSquare), 0});
            }
        }

        inline void pushQueenPromotions(ScoredMoveList& noisy, i32 offset, Bitboard board) {
            while (!board.empty()) {
                const auto dstSquare = board.popLowestSquare();
                const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

                noisy.push({Move::promotion(srcSquare, dstSquare, PieceType::kQueen), 0});
            }
        }

        inline void pushUnderpromotions(ScoredMoveList& quiet, i32 offset, Bitboard board) {
            while (!board.empty()) {
                const auto dstSquare = board.popLowestSquare();
                const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

                quiet.push({Move::promotion(srcSquare, dstSquare, PieceType::kKnight), 0});
                quiet.push({Move::promotion(srcSquare, dstSquare, PieceType::kRook), 0});
                quiet.push({Move::promotion(srcSquare, dstSquare, PieceType::kBishop), 0});
            }
        }

        inline void pushCastling(ScoredMoveList& dst, Square srcSquare, Square dstSquare) {
            dst.push({Move::castling(srcSquare, dstSquare), 0});
        }

        inline void pushEnPassants(ScoredMoveList& noisy, i32 offset, Bitboard board) {
            while (!board.empty()) {
                const auto dstSquare = board.popLowestSquare();
                const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

                noisy.push({Move::enPassant(srcSquare, dstSquare), 0});
            }
        }

        template <Color Us>
        void generatePawnsNoisy_(ScoredMoveList& noisy, const Position& pos, Bitboard dstMask) {
            constexpr auto Them = oppColor(Us);

            constexpr auto PromotionRank = boards::promotionRank<Us>();

            constexpr auto ForwardOffset = offsets::up<Us>();
            constexpr auto LeftOffset = offsets::upLeft<Us>();
            constexpr auto RightOffset = offsets::upRight<Us>();

            const auto& bbs = pos.bbs();

            const auto theirs = bbs.occupancy<Them>();

            const auto forwardDstMask = dstMask & PromotionRank & ~theirs;

            const auto pawns = bbs.pawns<Us>();

            const auto leftAttacks = pawns.template shiftUpLeftRelative<Us>() & dstMask;
            const auto rightAttacks = pawns.template shiftUpRightRelative<Us>() & dstMask;

            pushQueenPromotions(noisy, LeftOffset, leftAttacks & theirs & PromotionRank);
            pushQueenPromotions(noisy, RightOffset, rightAttacks & theirs & PromotionRank);

            const auto forwards = pawns.template shiftUpRelative<Us>() & forwardDstMask;
            pushQueenPromotions(noisy, ForwardOffset, forwards);

            pushStandards(noisy, LeftOffset, leftAttacks & theirs & ~PromotionRank);
            pushStandards(noisy, RightOffset, rightAttacks & theirs & ~PromotionRank);

            if (pos.enPassant() != Square::kNone) {
                const auto epMask = Bitboard::fromSquare(pos.enPassant());

                pushEnPassants(noisy, LeftOffset, leftAttacks & epMask);
                pushEnPassants(noisy, RightOffset, rightAttacks & epMask);
            }
        }

        inline void generatePawnsNoisy(ScoredMoveList& noisy, const Position& pos, Bitboard dstMask) {
            if (pos.toMove() == Color::kBlack) {
                generatePawnsNoisy_<Color::kBlack>(noisy, pos, dstMask);
            } else {
                generatePawnsNoisy_<Color::kWhite>(noisy, pos, dstMask);
            }
        }

        template <Color kUs>
        void generatePawnsQuiet_(ScoredMoveList& quiet, const BitboardSet& bbs, Bitboard dstMask, Bitboard occ) {
            constexpr auto Them = oppColor(kUs);

            constexpr auto PromotionRank = boards::promotionRank<kUs>();
            constexpr auto ThirdRank = boards::rank<kUs>(2);

            const auto ForwardOffset = offsets::up<kUs>();
            const auto DoubleOffset = ForwardOffset * 2;

            constexpr auto LeftOffset = offsets::upLeft<kUs>();
            constexpr auto RightOffset = offsets::upRight<kUs>();

            const auto theirs = bbs.occupancy<Them>();

            const auto forwardDstMask = dstMask & ~theirs;

            const auto pawns = bbs.pawns<kUs>();

            const auto leftAttacks = pawns.template shiftUpLeftRelative<kUs>() & dstMask;
            const auto rightAttacks = pawns.template shiftUpRightRelative<kUs>() & dstMask;

            pushUnderpromotions(quiet, LeftOffset, leftAttacks & theirs & PromotionRank);
            pushUnderpromotions(quiet, RightOffset, rightAttacks & theirs & PromotionRank);

            auto forwards = pawns.template shiftUpRelative<kUs>() & ~occ;

            auto singles = forwards & forwardDstMask;
            pushUnderpromotions(quiet, ForwardOffset, singles & PromotionRank);
            singles &= ~PromotionRank;

            forwards &= ThirdRank;
            const auto doubles = forwards.template shiftUpRelative<kUs>() & forwardDstMask;

            pushStandards(quiet, DoubleOffset, doubles);
            pushStandards(quiet, ForwardOffset, singles);
        }

        inline void generatePawnsQuiet(ScoredMoveList& quiet, const Position& pos, Bitboard dstMask, Bitboard occ) {
            if (pos.toMove() == Color::kBlack) {
                generatePawnsQuiet_<Color::kBlack>(quiet, pos.bbs(), dstMask, occ);
            } else {
                generatePawnsQuiet_<Color::kWhite>(quiet, pos.bbs(), dstMask, occ);
            }
        }

        template <PieceType kPiece, const std::array<Bitboard, 64>& kAttacks>
        inline void precalculated(ScoredMoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto us = pos.toMove();

            auto pieces = pos.bbs().forPiece(kPiece, us);
            while (!pieces.empty()) {
                const auto srcSquare = pieces.popLowestSquare();
                const auto attacks = kAttacks[static_cast<usize>(srcSquare)];

                pushStandards(dst, srcSquare, attacks & dstMask);
            }
        }

        void generateKnights(ScoredMoveList& dst, const Position& pos, Bitboard dstMask) {
            precalculated<PieceType::kKnight, attacks::kKnightAttacks>(dst, pos, dstMask);
        }

        inline void generateFrcCastling(
            ScoredMoveList& dst,
            const Position& pos,
            Bitboard occupancy,
            Square king,
            Square kingDst,
            Square rook,
            Square rookDst
        ) {
            const auto toKingDst = rayBetween(king, kingDst);
            const auto toRook = rayBetween(king, rook);

            const auto occ = occupancy ^ squareBit(king) ^ squareBit(rook);

            if ((occ & (toKingDst | toRook | squareBit(kingDst) | squareBit(rookDst))).empty()
                && !pos.anyAttacked(toKingDst | squareBit(kingDst), pos.opponent()))
            {
                pushCastling(dst, king, rook);
            }
        }

        template <bool kCastling>
        void generateKings(ScoredMoveList& dst, const Position& pos, Bitboard dstMask) {
            precalculated<PieceType::kKing, attacks::kKingAttacks>(dst, pos, dstMask);

            if constexpr (kCastling) {
                if (!pos.isCheck()) {
                    const auto& castlingRooks = pos.castlingRooks();
                    const auto occupancy = pos.bbs().occupancy();

                    // this branch is cheaper than the extra checks the chess960 castling movegen does
                    if (g_opts.chess960) {
                        if (pos.toMove() == Color::kBlack) {
                            if (castlingRooks.black().kingside != Square::kNone) {
                                generateFrcCastling(
                                    dst,
                                    pos,
                                    occupancy,
                                    pos.blackKing(),
                                    Square::kG8,
                                    castlingRooks.black().kingside,
                                    Square::kF8
                                );
                            }
                            if (castlingRooks.black().queenside != Square::kNone) {
                                generateFrcCastling(
                                    dst,
                                    pos,
                                    occupancy,
                                    pos.blackKing(),
                                    Square::kC8,
                                    castlingRooks.black().queenside,
                                    Square::kD8
                                );
                            }
                        } else {
                            if (castlingRooks.white().kingside != Square::kNone) {
                                generateFrcCastling(
                                    dst,
                                    pos,
                                    occupancy,
                                    pos.whiteKing(),
                                    Square::kG1,
                                    castlingRooks.white().kingside,
                                    Square::kF1
                                );
                            }
                            if (castlingRooks.white().queenside != Square::kNone) {
                                generateFrcCastling(
                                    dst,
                                    pos,
                                    occupancy,
                                    pos.whiteKing(),
                                    Square::kC1,
                                    castlingRooks.white().queenside,
                                    Square::kD1
                                );
                            }
                        }
                    } else {
                        if (pos.toMove() == Color::kBlack) {
                            if (castlingRooks.black().kingside != Square::kNone
                                && (occupancy & U64(0x6000000000000000)).empty()
                                && !pos.isAttacked(Square::kF8, Color::kWhite))
                            {
                                pushCastling(dst, pos.blackKing(), Square::kH8);
                            }

                            if (castlingRooks.black().queenside != Square::kNone
                                && (occupancy & U64(0x0E00000000000000)).empty()
                                && !pos.isAttacked(Square::kD8, Color::kWhite))
                            {
                                pushCastling(dst, pos.blackKing(), Square::kA8);
                            }
                        } else {
                            if (castlingRooks.white().kingside != Square::kNone
                                && (occupancy & U64(0x0000000000000060)).empty()
                                && !pos.isAttacked(Square::kF1, Color::kBlack))
                            {
                                pushCastling(dst, pos.whiteKing(), Square::kH1);
                            }

                            if (castlingRooks.white().queenside != Square::kNone
                                && (occupancy & U64(0x000000000000000E)).empty()
                                && !pos.isAttacked(Square::kD1, Color::kBlack))
                            {
                                pushCastling(dst, pos.whiteKing(), Square::kA1);
                            }
                        }
                    }
                }
            }
        }

        void generateSliders(ScoredMoveList& dst, const Position& pos, Bitboard dstMask) {
            const auto& bbs = pos.bbs();

            const auto us = pos.toMove();
            const auto them = oppColor(us);

            const auto ours = bbs.forColor(us);
            const auto theirs = bbs.forColor(them);

            const auto occupancy = ours | theirs;

            const auto queens = bbs.queens(us);

            auto rooks = queens | bbs.rooks(us);
            auto bishops = queens | bbs.bishops(us);

            while (!rooks.empty()) {
                const auto src = rooks.popLowestSquare();
                const auto attacks = attacks::getRookAttacks(src, occupancy);

                pushStandards(dst, src, attacks & dstMask);
            }

            while (!bishops.empty()) {
                const auto src = bishops.popLowestSquare();
                const auto attacks = attacks::getBishopAttacks(src, occupancy);

                pushStandards(dst, src, attacks & dstMask);
            }
        }
    } // namespace

    void generateNoisy(ScoredMoveList& noisy, const Position& pos) {
        const auto& bbs = pos.bbs();

        const auto us = pos.toMove();
        const auto them = oppColor(us);

        const auto ours = bbs.forColor(us);

        const auto kingDstMask = bbs.forColor(them);

        auto dstMask = kingDstMask;

        Bitboard epMask{};
        Bitboard epPawn{};

        if (pos.enPassant() != Square::kNone) {
            epMask = Bitboard::fromSquare(pos.enPassant());
            epPawn = us == Color::kBlack ? epMask.shiftUp() : epMask.shiftDown();
        }

        // queen promotions are noisy
        const auto promos = ~ours & (us == Color::kBlack ? boards::kRank1 : boards::kRank8);

        auto pawnDstMask = kingDstMask | epMask | promos;

        if (pos.isCheck()) {
            if (pos.checkers().multiple()) {
                generateKings<false>(noisy, pos, kingDstMask);
                return;
            }

            dstMask = pos.checkers();

            pawnDstMask = kingDstMask | (promos & rayBetween(pos.king(us), pos.checkers().lowestSquare()));

            // pawn that just moved is the checker
            if (!(pos.checkers() & epPawn).empty()) {
                pawnDstMask |= epMask;
            }
        }

        generateSliders(noisy, pos, dstMask);
        generatePawnsNoisy(noisy, pos, pawnDstMask);
        generateKnights(noisy, pos, dstMask);
        generateKings<false>(noisy, pos, kingDstMask);
    }

    void generateQuiet(ScoredMoveList& quiet, const Position& pos) {
        const auto& bbs = pos.bbs();

        const auto us = pos.toMove();
        const auto them = oppColor(us);

        const auto ours = bbs.forColor(us);
        const auto theirs = bbs.forColor(them);

        const auto kingDstMask = ~(ours | theirs);

        auto dstMask = kingDstMask;
        // for underpromotions
        auto pawnDstMask = kingDstMask;

        if (pos.isCheck()) {
            if (pos.checkers().multiple()) {
                generateKings<false>(quiet, pos, kingDstMask);
                return;
            }

            pawnDstMask = dstMask = rayBetween(pos.king(us), pos.checkers().lowestSquare());

            pawnDstMask |= pos.checkers() & boards::promotionRank(us);
        } else {
            pawnDstMask |= boards::promotionRank(us);
        }

        generateSliders(quiet, pos, dstMask);
        generatePawnsQuiet(quiet, pos, pawnDstMask, ours | theirs);
        generateKnights(quiet, pos, dstMask);
        generateKings<true>(quiet, pos, kingDstMask);
    }

    void generateAll(ScoredMoveList& dst, const Position& pos) {
        const auto& bbs = pos.bbs();

        const auto us = pos.toMove();

        const auto kingDstMask = ~bbs.forColor(pos.toMove());

        auto dstMask = kingDstMask;

        Bitboard epMask{};
        Bitboard epPawn{};

        if (pos.enPassant() != Square::kNone) {
            epMask = Bitboard::fromSquare(pos.enPassant());
            epPawn = pos.toMove() == Color::kBlack ? epMask.shiftUp() : epMask.shiftDown();
        }

        auto pawnDstMask = kingDstMask;

        if (pos.isCheck()) {
            if (pos.checkers().multiple()) {
                generateKings<false>(dst, pos, kingDstMask);
                return;
            }

            pawnDstMask = dstMask = pos.checkers() | rayBetween(pos.king(us), pos.checkers().lowestSquare());

            if (!(pos.checkers() & epPawn).empty()) {
                pawnDstMask |= epMask;
            }
        }

        generateSliders(dst, pos, dstMask);
        generatePawnsNoisy(dst, pos, pawnDstMask);
        generatePawnsQuiet(dst, pos, dstMask, bbs.occupancy());
        generateKnights(dst, pos, dstMask);
        generateKings<true>(dst, pos, kingDstMask);
    }
} // namespace stormphrax
