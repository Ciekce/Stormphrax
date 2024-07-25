/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include <array>
#include <algorithm>

#include "attacks/attacks.h"
#include "rays.h"
#include "util/bitfield.h"
#include "opts.h"

namespace stormphrax
{
	namespace
	{
		inline auto pushStandards(ScoredMoveList &dst, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				dst.push({Move::standard(srcSquare, dstSquare), 0});
			}
		}

		inline auto pushStandards(ScoredMoveList &dst, Square srcSquare, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				dst.push({Move::standard(srcSquare, dstSquare), 0});
			}
		}

		inline auto pushQueenPromotions(ScoredMoveList &noisy, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				noisy.push({Move::promotion(srcSquare, dstSquare, piece_types::Queen), 0});
			}
		}

		inline auto pushUnderpromotions(ScoredMoveList &quiet, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				quiet.push({Move::promotion(srcSquare, dstSquare, piece_types::Knight), 0});
				quiet.push({Move::promotion(srcSquare, dstSquare, piece_types::Rook), 0});
				quiet.push({Move::promotion(srcSquare, dstSquare, piece_types::Bishop), 0});
			}
		}

		inline auto pushCastling(ScoredMoveList &dst, Square srcSquare, Square dstSquare)
		{
			dst.push({Move::castling(srcSquare, dstSquare), 0});
		}

		inline auto pushEnPassants(ScoredMoveList &noisy, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				noisy.push({Move::enPassant(srcSquare, dstSquare), 0});
			}
		}

		template <u8 UsRaw>
		auto generatePawnsNoisy_(ScoredMoveList &noisy, const Position &pos, Bitboard dstMask)
		{
			constexpr auto Us = Color{UsRaw};
			constexpr auto Them = Us.opponent();

			constexpr auto PromotionRank = boards::promotionRank(Us);

			constexpr auto ForwardOffset = offsets::up(Us);
			constexpr auto LeftOffset = offsets::upLeft(Us);
			constexpr auto RightOffset = offsets::upRight(Us);

			const auto &bbs = pos.bbs();

			const auto theirs = bbs.occupancy(Them);

			const auto forwardDstMask = dstMask & PromotionRank & ~theirs;

			const auto pawns = bbs.pawns(Us);

			const auto leftAttacks = pawns.shiftUpLeftRelative(Us) & dstMask;
			const auto rightAttacks = pawns.shiftUpRightRelative(Us) & dstMask;

			pushQueenPromotions(noisy, LeftOffset,   leftAttacks & theirs & PromotionRank);
			pushQueenPromotions(noisy, RightOffset, rightAttacks & theirs & PromotionRank);

			const auto forwards = pawns.shiftUpRelative(Us) & forwardDstMask;
			pushQueenPromotions(noisy, ForwardOffset, forwards);

			pushStandards(noisy,  LeftOffset,  leftAttacks & theirs & ~PromotionRank);
			pushStandards(noisy, RightOffset, rightAttacks & theirs & ~PromotionRank);

			if (pos.enPassant() != Square::None)
			{
				const auto epMask = Bitboard::fromSquare(pos.enPassant());

				pushEnPassants(noisy,  LeftOffset,  leftAttacks & epMask);
				pushEnPassants(noisy, RightOffset, rightAttacks & epMask);
			}
		}

		inline auto generatePawnsNoisy(ScoredMoveList &noisy, const Position &pos, Bitboard dstMask)
		{
			if (pos.toMove() == colors::Black)
				generatePawnsNoisy_<colors::Black.raw()>(noisy, pos, dstMask);
			else generatePawnsNoisy_<colors::White.raw()>(noisy, pos, dstMask);
		}

		template <u8 UsRaw>
		auto generatePawnsQuiet_(ScoredMoveList &quiet, const BitboardSet &bbs, Bitboard dstMask, Bitboard occ)
		{
			constexpr auto Us = Color{UsRaw};
			constexpr auto Them = Us.opponent();

			constexpr auto PromotionRank = boards::promotionRank(Us);
			constexpr auto ThirdRank = boards::rank(Us, 2);

			const auto ForwardOffset = offsets::up(Us);
			const auto DoubleOffset = ForwardOffset * 2;

			constexpr auto  LeftOffset = offsets::upLeft (Us);
			constexpr auto RightOffset = offsets::upRight(Us);

			const auto theirs = bbs.occupancy(Them);

			const auto forwardDstMask = dstMask & ~theirs;

			const auto pawns = bbs.pawns(Us);

			const auto  leftAttacks = pawns.shiftUpLeftRelative (Us) & dstMask;
			const auto rightAttacks = pawns.shiftUpRightRelative(Us) & dstMask;

			pushUnderpromotions(quiet,  LeftOffset,  leftAttacks & theirs & PromotionRank);
			pushUnderpromotions(quiet, RightOffset, rightAttacks & theirs & PromotionRank);

			auto forwards = pawns.shiftUpRelative(Us) & ~occ;

			auto singles = forwards & forwardDstMask;
			pushUnderpromotions(quiet, ForwardOffset, singles & PromotionRank);
			singles &= ~PromotionRank;

			forwards &= ThirdRank;
			const auto doubles = forwards.shiftUpRelative(Us) & forwardDstMask;

			pushStandards(quiet,  DoubleOffset, doubles);
			pushStandards(quiet, ForwardOffset, singles);
		}

		inline auto generatePawnsQuiet(ScoredMoveList &quiet, const Position &pos, Bitboard dstMask, Bitboard occ)
		{
			if (pos.toMove() == colors::Black)
				generatePawnsQuiet_<colors::Black.raw()>(quiet, pos.bbs(), dstMask, occ);
			else generatePawnsQuiet_<colors::White.raw()>(quiet, pos.bbs(), dstMask, occ);
		}

		template <u8 PieceRaw, const std::array<Bitboard, 64> &Attacks>
		inline auto precalculated(ScoredMoveList &dst, const Position &pos, Bitboard dstMask)
		{
			constexpr auto P = PieceType::fromRaw(PieceRaw);

			const auto us = pos.toMove();

			auto pieces = pos.bbs().forPiece(P, us);
			while (!pieces.empty())
			{
				const auto srcSquare = pieces.popLowestSquare();
				const auto attacks = Attacks[static_cast<usize>(srcSquare)];

				pushStandards(dst, srcSquare, attacks & dstMask);
			}
		}

		auto generateKnights(ScoredMoveList &dst, const Position &pos, Bitboard dstMask)
		{
			precalculated<piece_types::Knight.raw(), attacks::KnightAttacks>(dst, pos, dstMask);
		}

		inline auto generateFrcCastling(ScoredMoveList &dst, const Position &pos, Bitboard occupancy,
			Square king, Square kingDst, Square rook, Square rookDst)
		{
			const auto toKingDst = rayBetween(king, kingDst);
			const auto toRook = rayBetween(king, rook);

			const auto occ = occupancy ^ squareBit(king) ^ squareBit(rook);

			if ((occ & (toKingDst | toRook | squareBit(kingDst) | squareBit(rookDst))).empty()
				&& !pos.anyAttacked(toKingDst | squareBit(kingDst), pos.opponent()))
				pushCastling(dst, king, rook);
		}

		template <bool Castling>
		auto generateKings(ScoredMoveList &dst, const Position &pos, Bitboard dstMask)
		{
			precalculated<piece_types::King.raw(), attacks::KingAttacks>(dst, pos, dstMask);

			if constexpr (Castling)
			{
				if (!pos.isCheck())
				{
					const auto &castlingRooks = pos.castlingRooks();
					const auto occupancy = pos.bbs().occupancy();

					// this branch is cheaper than the extra checks the chess960 castling movegen does
					if (g_opts.chess960)
					{
						if (pos.toMove() == colors::Black)
						{
							if (castlingRooks.black().kingside != Square::None)
								generateFrcCastling(dst, pos, occupancy,
									pos.blackKing(), Square::G8,
									castlingRooks.black().kingside, Square::F8);
							if (castlingRooks.black().queenside != Square::None)
								generateFrcCastling(dst, pos, occupancy,
									pos.blackKing(), Square::C8,
									castlingRooks.black().queenside, Square::D8);
						}
						else
						{
							if (castlingRooks.white().kingside != Square::None)
								generateFrcCastling(dst, pos, occupancy,
									pos.whiteKing(), Square::G1,
									castlingRooks.white().kingside, Square::F1);
							if (castlingRooks.white().queenside != Square::None)
								generateFrcCastling(dst, pos, occupancy,
									pos.whiteKing(), Square::C1,
									castlingRooks.white().queenside, Square::D1);
						}
					}
					else
					{
						if (pos.toMove() == colors::Black)
						{
							if (castlingRooks.black().kingside != Square::None
								&& (occupancy & U64(0x6000000000000000)).empty()
								&& !pos.isAttacked(Square::F8, colors::White))
								pushCastling(dst, pos.blackKing(), Square::H8);

							if (castlingRooks.black().queenside != Square::None
								&& (occupancy & U64(0x0E00000000000000)).empty()
								&& !pos.isAttacked(Square::D8, colors::White))
								pushCastling(dst, pos.blackKing(), Square::A8);
						}
						else
						{
							if (castlingRooks.white().kingside != Square::None
								&& (occupancy & U64(0x0000000000000060)).empty()
								&& !pos.isAttacked(Square::F1, colors::Black))
								pushCastling(dst, pos.whiteKing(), Square::H1);

							if (castlingRooks.white().queenside != Square::None
								&& (occupancy & U64(0x000000000000000E)).empty()
								&& !pos.isAttacked(Square::D1, colors::Black))
								pushCastling(dst, pos.whiteKing(), Square::A1);
						}
					}
				}
			}
		}

		auto generateSliders(ScoredMoveList &dst, const Position &pos, Bitboard dstMask)
		{
			const auto &bbs = pos.bbs();

			const auto us = pos.toMove();
			const auto them = us.opponent();

			const auto ours = bbs.forColor(us);
			const auto theirs = bbs.forColor(them);

			const auto occupancy = ours | theirs;

			const auto queens = bbs.queens(us);

			auto rooks = queens | bbs.rooks(us);
			auto bishops = queens | bbs.bishops(us);

			while (!rooks.empty())
			{
				const auto src = rooks.popLowestSquare();
				const auto attacks = attacks::getRookAttacks(src, occupancy);

				pushStandards(dst, src, attacks & dstMask);
			}

			while (!bishops.empty())
			{
				const auto src = bishops.popLowestSquare();
				const auto attacks = attacks::getBishopAttacks(src, occupancy);

				pushStandards(dst, src, attacks & dstMask);
			}
		}
	}

	auto generateNoisy(ScoredMoveList &noisy, const Position &pos) -> void
	{
		const auto &bbs = pos.bbs();

		const auto us = pos.toMove();
		const auto them = us.opponent();

		const auto ours = bbs.forColor(us);

		const auto kingDstMask = bbs.forColor(them);

		auto dstMask = kingDstMask;

		Bitboard epMask{};
		Bitboard epPawn{};

		if (pos.enPassant() != Square::None)
		{
			epMask = Bitboard::fromSquare(pos.enPassant());
			epPawn = us == colors::Black ? epMask.shiftUp() : epMask.shiftDown();
		}

		// queen promotions are noisy
		const auto promos = ~ours & (us == colors::Black ? boards::Rank1 : boards::Rank8);

		auto pawnDstMask = kingDstMask | epMask | promos;

		if (pos.isCheck())
		{
			if (pos.checkers().multiple())
			{
				generateKings<false>(noisy, pos, kingDstMask);
				return;
			}

			dstMask = pos.checkers();

			pawnDstMask = kingDstMask | (promos & rayBetween(pos.king(us), pos.checkers().lowestSquare()));

			// pawn that just moved is the checker
			if (!(pos.checkers() & epPawn).empty())
				pawnDstMask |= epMask;
		}

		generateSliders(noisy, pos, dstMask);
		generatePawnsNoisy(noisy, pos, pawnDstMask);
		generateKnights(noisy, pos, dstMask);
		generateKings<false>(noisy, pos, kingDstMask);
	}

	auto generateQuiet(ScoredMoveList &quiet, const Position &pos) -> void
	{
		const auto &bbs = pos.bbs();

		const auto us = pos.toMove();
		const auto them = us.opponent();

		const auto ours = bbs.forColor(us);
		const auto theirs = bbs.forColor(them);

		const auto kingDstMask = ~(ours | theirs);

		auto dstMask = kingDstMask;
		// for underpromotions
		auto pawnDstMask = kingDstMask;

		if (pos.isCheck())
		{
			if (pos.checkers().multiple())
			{
				generateKings<false>(quiet, pos, kingDstMask);
				return;
			}

			pawnDstMask = dstMask = rayBetween(pos.king(us), pos.checkers().lowestSquare());

			pawnDstMask |= pos.checkers() & boards::promotionRank(us);
		}
		else pawnDstMask |= boards::promotionRank(us);

		generateSliders(quiet, pos, dstMask);
		generatePawnsQuiet(quiet, pos, pawnDstMask, ours | theirs);
		generateKnights(quiet, pos, dstMask);
		generateKings<true>(quiet, pos, kingDstMask);
	}

	auto generateAll(ScoredMoveList &dst, const Position &pos) -> void
	{
		const auto &bbs = pos.bbs();

		const auto us = pos.toMove();

		const auto kingDstMask = ~bbs.forColor(pos.toMove());

		auto dstMask = kingDstMask;

		Bitboard epMask{};
		Bitboard epPawn{};

		if (pos.enPassant() != Square::None)
		{
			epMask = Bitboard::fromSquare(pos.enPassant());
			epPawn = pos.toMove() == colors::Black ? epMask.shiftUp() : epMask.shiftDown();
		}

		auto pawnDstMask = kingDstMask;

		if (pos.isCheck())
		{
			if (pos.checkers().multiple())
			{
				generateKings<false>(dst, pos, kingDstMask);
				return;
			}

			pawnDstMask = dstMask = pos.checkers()
				| rayBetween(pos.king(us), pos.checkers().lowestSquare());

			if (!(pos.checkers() & epPawn).empty())
				pawnDstMask |= epMask;
		}

		generateSliders(dst, pos, dstMask);
		generatePawnsNoisy(dst, pos, pawnDstMask);
		generatePawnsQuiet(dst, pos, dstMask, bbs.occupancy());
		generateKnights(dst, pos, dstMask);
		generateKings<true>(dst, pos, kingDstMask);
	}
}
