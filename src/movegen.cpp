/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#include "movegen.h"

#include <array>
#include <algorithm>

#include "attacks/attacks.h"
#include "rays.h"
#include "eval/material.h"
#include "util/bitfield.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include "uci.h"
#include "pretty.h"

namespace polaris
{
	namespace
	{
		inline void pushStandards(MoveList &dst, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				dst.push(Move::standard(srcSquare, dstSquare));
			}
		}

		inline void pushStandards(MoveList &dst, Square srcSquare, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				dst.push(Move::standard(srcSquare, dstSquare));
			}
		}

		inline void pushQueenPromotions(MoveList &noisy, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				noisy.push(Move::promotion(srcSquare, dstSquare, BasePiece::Queen));
			}
		}

		inline void pushUnderpromotions(MoveList &quiet, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				quiet.push(Move::promotion(srcSquare, dstSquare, BasePiece::Knight));
			//	quiet.push(Move::promotion(srcSquare, dstSquare, BasePiece::Rook));
			//	quiet.push(Move::promotion(srcSquare, dstSquare, BasePiece::Bishop));
			}
		}

		inline void pushCastling(MoveList &dst, Square srcSquare, Square dstSquare)
		{
			dst.push(Move::castling(srcSquare, dstSquare));
		}

		inline void pushEnPassants(MoveList &noisy, i32 offset, Bitboard board)
		{
			while (!board.empty())
			{
				const auto dstSquare = board.popLowestSquare();
				const auto srcSquare = static_cast<Square>(static_cast<i32>(dstSquare) - offset);

				noisy.push(Move::enPassant(srcSquare, dstSquare));
			}
		}

		template <Color Us>
		void generatePawnsNoisy_(MoveList &noisy, const Position &pos, Bitboard dstMask)
		{
			constexpr auto Them = oppColor(Us);

			constexpr auto PromotionRank = boards::promotionRank<Us>();

			constexpr auto ForwardOffset = offsets::up<Us>();
			constexpr auto LeftOffset = offsets::upLeft<Us>();
			constexpr auto RightOffset = offsets::upRight<Us>();

			const auto theirs = pos.occupancy<Them>();

			const auto forwardDstMask = dstMask & PromotionRank & ~theirs;

			const auto pawns = pos.pawns<Us>();

			const auto leftAttacks = pawns.template shiftUpLeftRelative<Us>() & dstMask;
			const auto rightAttacks = pawns.template shiftUpRightRelative<Us>() & dstMask;

			pushQueenPromotions(noisy, LeftOffset,   leftAttacks & theirs & PromotionRank);
			pushQueenPromotions(noisy, RightOffset, rightAttacks & theirs & PromotionRank);

			const auto forwards = pawns.template shiftUpRelative<Us>() & forwardDstMask;
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

		inline void generatePawnsNoisy(MoveList &noisy, const Position &pos, Bitboard dstMask)
		{
			if (pos.toMove() == Color::Black)
				generatePawnsNoisy_<Color::Black>(noisy, pos, dstMask);
			else generatePawnsNoisy_<Color::White>(noisy, pos, dstMask);
		}

		template <Color Us>
		void generatePawnsQuiet_(MoveList &quiet, const Position &pos, Bitboard dstMask, Bitboard occ)
		{
			constexpr auto Them = oppColor(Us);

			constexpr auto PromotionRank = boards::promotionRank<Us>();
			constexpr auto ThirdRank = boards::rank<Us>(2);

			const auto ForwardOffset = offsets::up<Us>();
			const auto DoubleOffset = ForwardOffset * 2;

			constexpr auto  LeftOffset = offsets::upLeft <Us>();
			constexpr auto RightOffset = offsets::upRight<Us>();

			const auto theirs = pos.occupancy<Them>();

			const auto forwardDstMask = dstMask & ~theirs;

			const auto pawns = pos.pawns<Us>();

			const auto  leftAttacks = pawns.template shiftUpLeftRelative <Us>() & dstMask;
			const auto rightAttacks = pawns.template shiftUpRightRelative<Us>() & dstMask;

			pushUnderpromotions(quiet,  LeftOffset,  leftAttacks & theirs & PromotionRank);
			pushUnderpromotions(quiet, RightOffset, rightAttacks & theirs & PromotionRank);

			auto forwards = pawns.template shiftUpRelative<Us>() & ~occ;

			auto singles = forwards & forwardDstMask;
			pushUnderpromotions(quiet, ForwardOffset, singles & PromotionRank);
			singles &= ~PromotionRank;

			forwards &= ThirdRank;
			const auto doubles = forwards.template shiftUpRelative<Us>() & forwardDstMask;

			pushStandards(quiet,  DoubleOffset, doubles);
			pushStandards(quiet, ForwardOffset, singles);
		}

		inline void generatePawnsQuiet(MoveList &quiet, const Position &pos, Bitboard dstMask, Bitboard occ)
		{
			if (pos.toMove() == Color::Black)
				generatePawnsQuiet_<Color::Black>(quiet, pos, dstMask, occ);
			else generatePawnsQuiet_<Color::White>(quiet, pos, dstMask, occ);
		}

		template <BasePiece Piece, const std::array<Bitboard, 64> &Attacks>
		inline void precalculated(MoveList &dst, const Position &pos, Bitboard dstMask)
		{
			const auto us = pos.toMove();
			auto pieces = pos.board(Piece, us);

			while (!pieces.empty())
			{
				const auto srcSquare = pieces.popLowestSquare();
				const auto attacks = Attacks[static_cast<size_t>(srcSquare)];

				pushStandards(dst, srcSquare, attacks & dstMask);
			}
		}

		void generateKnights(MoveList &dst, const Position &pos, Bitboard dstMask)
		{
			precalculated<BasePiece::Knight, attacks::KnightAttacks>(dst, pos, dstMask);
		}

		template <bool Castling>
		void generateKings(MoveList &dst, const Position &pos, Bitboard dstMask)
		{
			precalculated<BasePiece::King, attacks::KingAttacks>(dst, pos, dstMask);

			if constexpr(Castling)
			{
				if (!pos.isCheck())
				{
					const auto occupancy = pos.occupancy();

					if (pos.toMove() == Color::Black)
					{
						if (testFlags(pos.castling(), PositionFlags::BlackKingside)
							&& (occupancy & U64(0x6000000000000000)).empty()
							&& !pos.isAttacked(Square::F8, Color::White))
							pushCastling(dst, pos.blackKing(), Square::G8);

						if (testFlags(pos.castling(), PositionFlags::BlackQueenside)
							&& (occupancy & U64(0x0E00000000000000)).empty()
							&& !pos.isAttacked(Square::D8, Color::White))
							pushCastling(dst, pos.blackKing(), Square::C8);
					}
					else
					{
						if (testFlags(pos.castling(), PositionFlags::WhiteKingside)
							&& (occupancy & U64(0x0000000000000060)).empty()
							&& !pos.isAttacked(Square::F1, Color::Black))
							pushCastling(dst, pos.whiteKing(), Square::G1);

						if (testFlags(pos.castling(), PositionFlags::WhiteQueenside)
							&& (occupancy & U64(0x000000000000000E)).empty()
							&& !pos.isAttacked(Square::D1, Color::Black))
							pushCastling(dst, pos.whiteKing(), Square::C1);
					}
				}
			}
		}

		void generateSliders(MoveList &dst, const Position &pos, Bitboard dstMask)
		{
			const auto us = pos.toMove();
			const auto them = oppColor(us);

			const auto ours = pos.occupancy(us);
			const auto theirs = pos.occupancy(them);

			const auto occupancy = ours | theirs;

			const auto queens = pos.queens(us);

			auto rooks = queens | pos.rooks(us);
			auto bishops = queens | pos.bishops(us);

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

	void generateNoisy(MoveList &noisy, const Position &pos)
	{
		const auto us = pos.toMove();
		const auto them = oppColor(us);

		const auto ours = pos.occupancy(us);

		const auto kingDstMask = pos.occupancy(them);

		auto dstMask = kingDstMask;

		Bitboard epMask{};
		Bitboard epPawn{};

		if (pos.enPassant() != Square::None)
		{
			epMask = Bitboard::fromSquare(pos.enPassant());
			epPawn = us == Color::Black ? epMask.shiftUp() : epMask.shiftDown();
		}

		// queen promotions are noisy
		const auto promos = ~ours & (us == Color::Black ? boards::Rank1 : boards::Rank8);

		auto pawnDstMask = kingDstMask | epMask | promos;

		if (pos.isCheck())
		{
			if (pos.checkers().multiple())
			{
				generateKings<false>(noisy, pos, kingDstMask);
				return;
			}

			dstMask = pos.checkers();

			pawnDstMask = kingDstMask | (promos & rayTo(us == Color::Black ? pos.blackKing() : pos.whiteKing(),
				pos.checkers().lowestSquare()));

			// pawn that just moved is the checker
			if (!(pos.checkers() & epPawn).empty())
				pawnDstMask |= epMask;
		}

		generateSliders(noisy, pos, dstMask);
		generatePawnsNoisy(noisy, pos, pawnDstMask);
		generateKnights(noisy, pos, dstMask);
		generateKings<false>(noisy, pos, kingDstMask);
	}

	void generateQuiet(MoveList &quiet, const Position &pos)
	{
		const auto us = pos.toMove();
		const auto them = oppColor(us);

		const auto ours = pos.occupancy(us);
		const auto theirs = pos.occupancy(them);

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

			pawnDstMask = dstMask = rayTo(us == Color::Black ? pos.blackKing() : pos.whiteKing(),
				pos.checkers().lowestSquare());

			pawnDstMask |= pos.checkers() & boards::promotionRank(us);
		}
		else pawnDstMask |= boards::promotionRank(us);

		generateSliders(quiet, pos, dstMask);
		generatePawnsQuiet(quiet, pos, pawnDstMask, ours | theirs);
		generateKnights(quiet, pos, dstMask);
		generateKings<true>(quiet, pos, kingDstMask);
	}

	void generateAll(MoveList &dst, const Position &pos)
	{
		const auto us = pos.toMove();

		const auto kingDstMask = ~pos.occupancy(pos.toMove());

		auto dstMask = kingDstMask;

		Bitboard epMask{};
		Bitboard epPawn{};

		if (pos.enPassant() != Square::None)
		{
			epMask = Bitboard::fromSquare(pos.enPassant());
			epPawn = pos.toMove() == Color::Black ? epMask.shiftUp() : epMask.shiftDown();
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
				| rayTo(us == Color::Black ? pos.blackKing() : pos.whiteKing(),
					pos.checkers().lowestSquare());

			if (!(pos.checkers() & epPawn).empty())
				pawnDstMask |= epMask;
		}

		generateSliders(dst, pos, dstMask);
		generatePawnsNoisy(dst, pos, pawnDstMask);
		generatePawnsQuiet(dst, pos, dstMask, pos.occupancy());
		generateKnights(dst, pos, dstMask);
		generateKings<true>(dst, pos, kingDstMask);
	}

	void orderMoves(MoveList &moves, const Position &pos, Move first)
	{
		std::sort(moves.begin(), moves.end(), [&pos, first](const auto a, const auto b)
		{
			if (a == first)
				return true;
			if (b == first)
				return false;

			const auto aDstPiece = pos.pieceAt(a.dst());
			const auto bDstPiece = pos.pieceAt(b.dst());

			const bool aCapture = aDstPiece != Piece::None;
			const bool bCapture = bDstPiece != Piece::None;

		//	const bool aPromotion = a.type() == MoveType::Promotion;
		//	const bool bPromotion = b.type() == MoveType::Promotion;

			if (aCapture && bCapture)
			{
		//		if (aPromotion && !bPromotion)
		//			return true;

				const auto aSrcPiece = pos.pieceAt(a.src());
				const auto bSrcPiece = pos.pieceAt(b.src());

				const auto aCapturedValue = eval::pieceValue(aDstPiece).midgame;
				const auto bCapturedValue = eval::pieceValue(bDstPiece).midgame;

				const auto aDelta = aCapturedValue - eval::pieceValue(aSrcPiece).midgame;
				const auto bDelta = bCapturedValue - eval::pieceValue(bSrcPiece).midgame;

				if (aDelta > bDelta || (aDelta == bDelta && aCapturedValue > bCapturedValue))
					return true;
				else if (aDelta < bDelta)
					return false;
			}
			else if (aCapture)
				return true;

		//	if (aPromotion && !bPromotion)
		//		return true;

			return false;
		});
	}
}
