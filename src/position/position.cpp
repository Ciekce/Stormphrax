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

#include "position.h"

#ifndef NDEBUG
#include <iostream>
#include <iomanip>
#include "../uci.h"
#include "../pretty.h"
#endif
#include <algorithm>
#include <sstream>

#include "../hash.h"
#include "../eval/material.h"
#include "../util/parse.h"
#include "../util/split.h"
#include "../attacks/attacks.h"
#include "../movegen.h"
#include "../opts.h"
#include "../rays.h"

namespace polaris
{
	namespace
	{
#ifndef NDEBUG
		constexpr bool VerifyAll = true;
#endif

		constexpr auto PhaseInc = std::array{0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0, 0};
		constexpr auto PhaseIncBase = std::array{0, 1, 1, 2, 4, 0, 0};
	}

	HistoryGuard::~HistoryGuard()
	{
		m_pos.popMove();
	}

	template void Position::applyMoveUnchecked<false, false>(Move move, TTable *prefetchTt);
	template void Position::applyMoveUnchecked<true, false>(Move move, TTable *prefetchTt);
	template void Position::applyMoveUnchecked<false, true>(Move move, TTable *prefetchTt);
	template void Position::applyMoveUnchecked<true, true>(Move move, TTable *prefetchTt);

	template Piece Position::setPiece<false, false>(Square, Piece);
	template Piece Position::setPiece<true, false>(Square, Piece);
	template Piece Position::setPiece<false, true>(Square, Piece);
	template Piece Position::setPiece<true, true>(Square, Piece);

	template Piece Position::removePiece<false, false>(Square);
	template Piece Position::removePiece<true, false>(Square);
	template Piece Position::removePiece<false, true>(Square);
	template Piece Position::removePiece<true, true>(Square);

	template Piece Position::movePiece<false, false>(Square, Square);
	template Piece Position::movePiece<true, false>(Square, Square);
	template Piece Position::movePiece<false, true>(Square, Square);
	template Piece Position::movePiece<true, true>(Square, Square);

	template Piece Position::promotePawn<false, false>(Square, Square, BasePiece);
	template Piece Position::promotePawn<true, false>(Square, Square, BasePiece);
	template Piece Position::promotePawn<false, true>(Square, Square, BasePiece);
	template Piece Position::promotePawn<true, true>(Square, Square, BasePiece);

	template void Position::castle<false, false>(Square, Square);
	template void Position::castle<true, false>(Square, Square);
	template void Position::castle<false, true>(Square, Square);
	template void Position::castle<true, true>(Square, Square);

	template Piece Position::enPassant<false, false>(Square, Square);
	template Piece Position::enPassant<true, false>(Square, Square);
	template Piece Position::enPassant<false, true>(Square, Square);
	template Piece Position::enPassant<true, true>(Square, Square);

	template void Position::regen<false>();
	template void Position::regen<true>();

#ifndef NDEBUG
	template bool Position::verify<false, false>();
	template bool Position::verify<true, false>();
	template bool Position::verify<false, true>();
	template bool Position::verify<true, true>();
#endif

	Position::Position(bool init)
	{
		m_states.reserve(256);

		if (init)
			m_states.push_back({});
	}

	template <bool UpdateMaterial, bool History>
	void Position::applyMoveUnchecked(Move move, TTable *prefetchTt)
	{
		auto &prevState = currState();

		prevState.lastMove = move;

		if constexpr (History)
			m_states.push_back(prevState);

		auto &state = currState();

		m_blackToMove = !m_blackToMove;

		state.key ^= hash::color();
		state.pawnKey ^= hash::color();

		if (state.enPassant != Square::None)
		{
			state.key ^= hash::enPassant(state.enPassant);
			state.enPassant = Square::None;
		}

		if (!move)
		{
#ifndef NDEBUG
			if constexpr (VerifyAll)
			{
				if (!verify<UpdateMaterial, History>())
				{
					printHistory(move);
					__builtin_trap();
				}
			}
#endif

			return;
		}

		const auto moveType = move.type();

		const auto moveSrc = move.src();
		const auto moveDst = move.dst();

		const auto currColor = opponent();

		if (currColor == Color::Black)
			++m_fullmove;

		auto newCastlingRooks = state.castlingRooks;

		const auto moving = state.boards.pieceAt(moveSrc);

#ifndef NDEBUG
		if (moving == Piece::None)
		{
			std::cerr << "corrupt board state" << std::endl;
			printHistory(move);
			__builtin_trap();
		}
#endif

		if (moving == Piece::BlackRook)
		{
			if (moveSrc == state.castlingRooks.blackShort)
				newCastlingRooks.blackShort = Square::None;
			if (moveSrc == state.castlingRooks.blackLong)
				newCastlingRooks.blackLong = Square::None;
		}
		else if (moving == Piece::WhiteRook)
		{
			if (moveSrc == state.castlingRooks.whiteShort)
				newCastlingRooks.whiteShort = Square::None;
			if (moveSrc == state.castlingRooks.whiteLong)
				newCastlingRooks.whiteLong = Square::None;
		}
		else if (moving == Piece::BlackKing)
			newCastlingRooks.blackShort = newCastlingRooks.blackLong = Square::None;
		else if (moving == Piece::WhiteKing)
			newCastlingRooks.whiteShort = newCastlingRooks.whiteLong = Square::None;
		else if (moving == Piece::BlackPawn && move.srcRank() == 6 && move.dstRank() == 4)
		{
			state.enPassant = toSquare(5, move.srcFile());
			state.key ^= hash::enPassant(state.enPassant);
		}
		else if (moving == Piece::WhitePawn && move.srcRank() == 1 && move.dstRank() == 3)
		{
			state.enPassant = toSquare(2, move.srcFile());
			state.key ^= hash::enPassant(state.enPassant);
		}

		prevState.captured = Piece::None;

		switch (moveType)
		{
		case MoveType::Standard: prevState.captured = movePiece<true, UpdateMaterial>(moveSrc, moveDst); break;
		case MoveType::Promotion: prevState.captured = promotePawn<true, UpdateMaterial>(moveSrc, moveDst, move.target()); break;
		case MoveType::Castling: castle<true, UpdateMaterial>(moveSrc, moveDst); break;
		case MoveType::EnPassant: prevState.captured = enPassant<true, UpdateMaterial>(moveSrc, moveDst); break;
		}

		if (prevState.captured == Piece::None
			&& basePiece(moving) != BasePiece::Pawn)
			++state.halfmove;
		else state.halfmove = 0;

		if (prevState.captured == Piece::BlackRook)
		{
			if (moveDst == state.castlingRooks.blackShort)
				newCastlingRooks.blackShort = Square::None;
			if (moveDst == state.castlingRooks.blackLong)
				newCastlingRooks.blackLong = Square::None;
		}
		else if (prevState.captured == Piece::WhiteRook)
		{
			if (moveDst == state.castlingRooks.whiteShort)
				newCastlingRooks.whiteShort = Square::None;
			if (moveDst == state.castlingRooks.whiteLong)
				newCastlingRooks.whiteLong = Square::None;
		}
		else if (moveType == MoveType::Castling)
		{
			if (pieceColor(moving) == Color::Black)
				newCastlingRooks.blackShort = newCastlingRooks.blackLong = Square::None;
			else newCastlingRooks.whiteShort = newCastlingRooks.whiteLong = Square::None;
		}

		if (newCastlingRooks != state.castlingRooks)
		{
			state.key ^= hash::castling(newCastlingRooks);
			state.key ^= hash::castling(state.castlingRooks);

			state.castlingRooks = newCastlingRooks;
		}

		if (prefetchTt)
			prefetchTt->prefetch(state.key);

		state.checkers = calcCheckers();

		state.phase = std::clamp(state.phase, 0, 24);

#ifndef NDEBUG
		if constexpr (VerifyAll)
		{
			if (!verify<UpdateMaterial, History>())
			{
				printHistory();
				__builtin_trap();
			}
		}
#endif
	}

	void Position::popMove()
	{
#ifndef NDEBUG
		if (m_states.size() <= 1)
		{
			std::cerr << "popMove() with no previous move?" << std::endl;
			return;
		}
#endif

		m_states.pop_back();

		m_blackToMove = !m_blackToMove;

		if (!currState().lastMove)
			return;

		if (toMove() == Color::Black)
			--m_fullmove;
	}

	bool Position::isPseudolegal(Move move) const
	{
		const auto &state = currState();

		const auto us = toMove();

		const auto src = move.src();
		const auto srcPiece = state.boards.pieceAt(src);

		if (srcPiece == Piece::None || pieceColor(srcPiece) != us)
			return false;

		const auto type = move.type();

		const auto dst = move.dst();
		const auto dstPiece = state.boards.pieceAt(dst);

		// we're capturing something
		if (dstPiece != Piece::None
			// we're capturing our own piece    and either not castling
			&& ((pieceColor(dstPiece) == us && (type != MoveType::Castling
					// or trying to castle with a non-rook
					|| dstPiece != colorPiece(BasePiece::Rook, us)))
				// or trying to capture a king
				|| basePiece(dstPiece) == BasePiece::King))
			return false;

		// take advantage of evasion generation if in check
		if (isCheck())
		{
			ScoredMoveList moves{};
			generateAll(moves, *this);

			return std::any_of(moves.begin(), moves.end(),
				[move](const auto m) { return m.move == move; });
		}

		const auto base = basePiece(srcPiece);
		const auto them = oppColor(us);
		const auto occ = state.boards.occupancy();

		if (type == MoveType::Castling)
		{
			if (base != BasePiece::King || isCheck())
				return false;

			const auto homeRank = relativeRank(us, 0);

			// wrong rank
			if (move.srcRank() != homeRank || move.dstRank() != homeRank)
				return false;

			const auto rank = squareRank(src);

			Square kingDst, rookDst;

			if (squareFile(src) < squareFile(dst))
			{
				// no castling rights
				if (dst != (us == Color::Black ? state.castlingRooks.blackShort : state.castlingRooks.whiteShort))
					return false;

				kingDst = toSquare(rank, 6);
				rookDst = toSquare(rank, 5);
			}
			else
			{
				// no castling rights
				if (dst != (us == Color::Black ? state.castlingRooks.blackLong : state.castlingRooks.whiteLong))
					return false;

				kingDst = toSquare(rank, 2);
				rookDst = toSquare(rank, 3);
			}

			// same checks as for movegen
			if (g_opts.chess960)
			{
				const auto toKingDst = rayBetween(src, kingDst);
				const auto toRook = rayBetween(src, dst);

				const auto castleOcc = occ ^ squareBit(src) ^ squareBit(dst);

				return (castleOcc & (toKingDst | toRook | squareBit(kingDst) | squareBit(rookDst))).empty()
					&& !anyAttacked(toKingDst | squareBit(kingDst), them);
			}
			else
			{
				if (dst == state.castlingRooks.blackShort)
					return (occ & U64(0x6000000000000000)).empty()
						&& !isAttacked(Square::F8, Color::White);
				else if (dst == state.castlingRooks.blackLong)
					return (occ & U64(0x0E00000000000000)).empty()
						&& !isAttacked(Square::D8, Color::White);
				else if (dst == state.castlingRooks.whiteShort)
					return (occ & U64(0x0000000000000060)).empty()
						&& !isAttacked(Square::F1, Color::Black);
				else return (occ & U64(0x000000000000000E)).empty()
						&& !isAttacked(Square::D1, Color::Black);
			}
		}

		if (base == BasePiece::Pawn)
		{
			if (type == MoveType::EnPassant && state.enPassant == Square::None)
				return false;

			const auto srcRank = move.srcRank();
			const auto dstRank = move.dstRank();

			// backwards move
			if ((us == Color::Black && dstRank >= srcRank)
				|| (us == Color::White && dstRank <= srcRank))
				return false;

			const auto promoRank = relativeRank(us, 7);

			// non-promotion move to back rank, or promotion move to any other rank
			if ((type == MoveType::Promotion) != (dstRank == promoRank))
				return false;

			// sideways move
			if (move.srcFile() != move.dstFile())
			{
				// not valid attack
				if (!(attacks::getPawnAttacks(src, us)
					& (state.boards.forColor(them) | squareBitChecked(state.enPassant)))[dst])
					return false;
			}
			// forward move onto a piece
			else if (dstPiece != Piece::None)
				return false;

			const auto delta = std::abs(dstRank - srcRank);

			i32 maxDelta;
			if (us == Color::Black)
				maxDelta = srcRank == 6 ? 2 : 1;
			else maxDelta = srcRank == 1 ? 2 : 1;

			if (delta > maxDelta)
				return false;

			if (delta == 2
				&& occ[static_cast<Square>(static_cast<i32>(dst) + (us == Color::White ? offsets::Down : offsets::Up))])
				return false;
		}
		else
		{
			if (type == MoveType::Promotion || type == MoveType::EnPassant)
				return false;

			Bitboard attacks{};

			switch (base)
			{
			case BasePiece::Knight: attacks = attacks::getKnightAttacks(src); break;
			case BasePiece::Bishop: attacks = attacks::getBishopAttacks(src, occ); break;
			case BasePiece::  Rook: attacks = attacks::getRookAttacks(src, occ); break;
			case BasePiece:: Queen: attacks = attacks::getQueenAttacks(src, occ); break;
			case BasePiece::  King: attacks = attacks::getKingAttacks(src); break;
			default: __builtin_unreachable();
			}

			if (!attacks[dst])
				return false;
		}

		return true;
	}

	std::string Position::toFen() const
	{
		const auto &state = currState();

		std::ostringstream fen{};

		for (i32 rank = 7; rank >= 0; --rank)
		{
			for (i32 file = 0; file < 8; ++file)
			{
				if (state.boards.pieceAt(rank, file) == Piece::None)
				{
					u32 emptySquares = 1;
					for (; file < 7 && state.boards.pieceAt(rank, file + 1) == Piece::None; ++file, ++emptySquares) {}

					fen << static_cast<char>('0' + emptySquares);
				}
				else fen << pieceToChar(state.boards.pieceAt(rank, file));
			}

			if (rank > 0)
				fen << '/';
		}

		fen << (toMove() == Color::White ? " w " : " b ");

		if (state.castlingRooks == CastlingRooks{})
			fen << '-';
		else if (g_opts.chess960)
		{
			constexpr auto BlackFiles = std::array{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
			constexpr auto WhiteFiles = std::array{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

			if (state.castlingRooks.whiteShort != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.whiteShort)];
			if (state.castlingRooks.whiteLong != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.whiteLong)];
			if (state.castlingRooks.blackShort != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.blackShort)];
			if (state.castlingRooks.blackLong != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.blackLong)];
		}
		else
		{
			if (state.castlingRooks.whiteShort != Square::None)
				fen << 'K';
			if (state.castlingRooks.whiteLong  != Square::None)
				fen << 'Q';
			if (state.castlingRooks.blackShort != Square::None)
				fen << 'k';
			if (state.castlingRooks.blackLong  != Square::None)
				fen << 'q';
		}

		if (state.enPassant != Square::None)
			fen << ' ' << squareToString(state.enPassant);
		else fen << " -";

		fen << ' ' << state.halfmove;
		fen << ' ' << m_fullmove;

		return fen.str();
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::setPiece(Square square, Piece piece)
	{
		auto &state = currState();

		const auto captured = state.boards.pieceAt(square);

		if (captured != Piece::None)
		{
			state.boards.removePiece(square, captured);

			state.phase -= PhaseInc[static_cast<i32>(captured)];

			if constexpr (UpdateMaterial)
				state.material -= eval::pieceSquareValue(captured, square);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, square);
				state.key ^= hash;

				if (basePiece(captured) == BasePiece::Pawn)
					state.pawnKey ^= hash;
			}
		}

		state.boards.setPiece(square, piece);

		if (piece == Piece::BlackKing)
			state.blackKing = square;
		else if (piece == Piece::WhiteKing)
			state.whiteKing = square;

		state.phase += PhaseInc[static_cast<usize>(piece)];

		if constexpr (UpdateMaterial)
			state.material += eval::pieceSquareValue(piece, square);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, square);
			state.key ^= hash;

			if (basePiece(piece) == BasePiece::Pawn)
				state.pawnKey ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::removePiece(Square square)
	{
		auto &state = currState();

		const auto piece = state.boards.pieceAt(square);

		if (piece != Piece::None)
		{
			state.boards.removePiece(square, piece);

			state.phase -= PhaseInc[static_cast<usize>(piece)];

			if constexpr (UpdateMaterial)
				state.material -= eval::pieceSquareValue(piece, square);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(piece, square);
				state.key ^= hash;
				if (basePiece(piece) == BasePiece::Pawn)
					state.pawnKey ^= hash;
			}
		}

		return piece;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::movePiece(Square src, Square dst)
	{
		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			state.boards.removePiece(dst, captured);
			state.phase -= PhaseInc[static_cast<usize>(captured)];

			if constexpr (UpdateMaterial)
				state.material -= eval::pieceSquareValue(captured, dst);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, dst);
				state.key ^= hash;
				if (basePiece(captured) == BasePiece::Pawn)
					state.pawnKey ^= hash;
			}
		}

		const auto piece = state.boards.pieceAt(src);

		state.boards.movePiece(src, dst, piece);

		if (piece == Piece::BlackKing)
			state.blackKing = dst;
		else if (piece == Piece::WhiteKing)
			state.whiteKing = dst;

		if constexpr (UpdateMaterial)
			state.material += eval::pieceSquareValue(piece, dst) - eval::pieceSquareValue(piece, src);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, src) ^ hash::pieceSquare(piece, dst);
			state.key ^= hash;
			if (basePiece(piece) == BasePiece::Pawn)
				state.pawnKey ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::promotePawn(Square src, Square dst, BasePiece target)
	{
		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			state.boards.removePiece(dst, captured);

			state.phase -= PhaseInc[static_cast<usize>(captured)];

			if constexpr (UpdateMaterial)
				state.material -= eval::pieceSquareValue(captured, dst);

			// cannot capture a pawn when promoting
			if constexpr (UpdateKey)
				state.key ^= hash::pieceSquare(captured, dst);
		}

		const auto color = state.boards.blackOccupancy()[src] ? Color::Black : Color::White;
		const auto pawn = colorPiece(BasePiece::Pawn, color);

		state.boards.moveAndChangePiece(src, dst, pawn, target);

		if constexpr(UpdateMaterial || UpdateKey)
		{
			const auto coloredTarget = colorPiece(target, color);

			if constexpr (UpdateMaterial)
				state.material += eval::pieceSquareValue(coloredTarget, dst)
					- eval::pieceSquareValue(pawn, src);

			if constexpr (UpdateKey)
			{
				const auto pawnHash = hash::pieceSquare(pawn, src);
				state.key ^= pawnHash ^ hash::pieceSquare(coloredTarget, dst);
				state.pawnKey ^= pawnHash;
			}
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	void Position::castle(Square kingSrc, Square rookSrc)
	{
		const auto rank = squareRank(kingSrc);

		Square kingDst, rookDst;

		if (squareFile(kingSrc) < squareFile(rookSrc))
		{
			// short
			kingDst = toSquare(rank, 6);
			rookDst = toSquare(rank, 5);
		}
		else
		{
			// long
			kingDst = toSquare(rank, 2);
			rookDst = toSquare(rank, 3);
		}

		if (g_opts.chess960)
		{
			const auto rook = removePiece<UpdateKey, UpdateMaterial>(rookSrc);

			if (kingSrc != kingDst)
				movePiece<UpdateKey, UpdateMaterial>(kingSrc, kingDst);

			setPiece<UpdateKey, UpdateMaterial>(rookDst, rook);
		}
		else
		{
			movePiece<UpdateKey, UpdateMaterial>(kingSrc, kingDst);
			movePiece<UpdateKey, UpdateMaterial>(rookSrc, rookDst);
		}
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::enPassant(Square src, Square dst)
	{
		auto &state = currState();

		const auto pawn = state.boards.pieceAt(src);
		const auto color = pieceColor(pawn);

		state.boards.movePiece(src, dst, pawn);

		if constexpr (UpdateMaterial)
			state.material += eval::pieceSquareValue(pawn, dst)
				- eval::pieceSquareValue(pawn, src);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(pawn, src) ^ hash::pieceSquare(pawn, dst);
			state.key ^= hash;
			state.pawnKey ^= hash;
		}

		auto rank = squareRank(dst);
		const auto file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto captureSquare = toSquare(rank, file);
		const auto enemyPawn = state.boards.pieceAt(captureSquare);

		state.boards.removePiece(captureSquare, enemyPawn);

		// pawns do not affect game phase

		if constexpr (UpdateMaterial)
			state.material -= eval::pieceSquareValue(enemyPawn, captureSquare);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(enemyPawn, captureSquare);
			state.key ^= hash;
			state.pawnKey ^= hash;
		}

		return enemyPawn;
	}

	void Position::regenMaterial()
	{
		auto &state = currState();

		state.material = TaperedScore{};

		auto occ = state.boards.occupancy();
		while (occ)
		{
			const auto square = occ.popLowestSquare();
			const auto piece = state.boards.pieceAt(square);

			state.material += eval::pieceSquareValue(piece, square);
		}
	}

	template <bool EnPassantFromMoves>
	void Position::regen()
	{
		auto &state = currState();

		state.phase = 0;
		state.key = 0;
		state.pawnKey = 0;

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				const auto square = toSquare(rank, file);
				if (const auto piece = state.boards.pieceAt(square); piece != Piece::None)
				{
					if (piece == Piece::BlackKing)
						state.blackKing = square;
					else if (piece == Piece::WhiteKing)
						state.whiteKing = square;

					state.phase += PhaseInc[static_cast<i32>(piece)];

					const auto hash = hash::pieceSquare(piece, toSquare(rank, file));
					state.key ^= hash;

					if (basePiece(piece) == BasePiece::Pawn)
						state.pawnKey ^= hash;
				}
			}
		}

		regenMaterial();

		if (state.phase > 24)
			state.phase = 24;

		if constexpr (EnPassantFromMoves)
		{
			state.enPassant = Square::None;

			if (m_states.size() > 1)
			{
				const auto lastMove = m_states[m_states.size() - 2].lastMove;

				if (lastMove && lastMove.type() == MoveType::Standard)
				{
					const auto piece = state.boards.pieceAt(lastMove.dst());

					if (basePiece(piece) == BasePiece::Pawn
						&& std::abs(lastMove.srcRank() - lastMove.dstRank()) == 2)
					{
						state.enPassant = toSquare(lastMove.dstRank()
							+ (piece == Piece::BlackPawn ? 1 : -1), lastMove.dstFile());
					}
				}
			}
		}

		const auto colorHash = hash::color(toMove());
		state.key ^= colorHash;
		state.pawnKey ^= colorHash;

		state.key ^= hash::castling(state.castlingRooks);
		state.key ^= hash::enPassant(state.enPassant);

		state.checkers = calcCheckers();
	}

#ifndef NDEBUG
	void Position::printHistory(Move last)
	{
		for (usize i = 0; i < m_states.size() - 1; ++i)
		{
			if (i != 0)
				std::cerr << ' ';
			std::cerr << uci::moveAndTypeToString(m_states[i].lastMove);
		}

		if (last)
		{
			if (!m_states.empty())
				std::cerr << ' ';
			std::cerr << uci::moveAndTypeToString(last);
		}

		std::cerr << std::endl;
	}

	template <bool CheckMaterial, bool HasHistory>
	bool Position::verify()
	{
		Position regened{*this};
		regened.regen<HasHistory>();

		std::ostringstream out{};
		out << std::hex << std::uppercase;

		bool failed = false;

#define PS_CHECK(A, B, Str) \
		if ((A) != (B)) \
		{ \
			out << "info string " Str " do not match"; \
			out << "\ninfo string current: " << std::setw(16) << std::setfill('0') << (A); \
			out << "\ninfo string regened: " << std::setw(16) << std::setfill('0') << (B); \
			out << '\n'; \
			failed = true; \
		}

		out << std::dec;
		PS_CHECK(static_cast<u64>(currState().enPassant), static_cast<u64>(regened.currState().enPassant), "en passant squares")
		out << std::hex;

		PS_CHECK(currState().key, regened.currState().key, "keys")
		PS_CHECK(currState().pawnKey, regened.currState().pawnKey, "pawn keys")

		out << std::dec;

		if constexpr (CheckMaterial)
		{
			if (currState().material != regened.currState().material)
			{
				out << "info string material scores do not match";
				out << "\ninfo string current: ";
				printScore(out, currState().material);
				out << "\ninfo string regened: ";
				printScore(out, regened.currState().material);
				out << '\n';
				failed = true;
			}
		}

#undef PS_CHECK_PIECES
#undef PS_CHECK_PIECE
#undef PS_CHECK

		if (failed)
			std::cout << out.view() << std::flush;

		return !failed;
	}
#endif

	Move Position::moveFromUci(const std::string &move) const
	{
		if (move.length() < 4 || move.length() > 5)
			return NullMove;

		const auto src = squareFromString(move.substr(0, 2));
		const auto dst = squareFromString(move.substr(2, 2));

		if (move.length() == 5)
			return Move::promotion(src, dst, basePieceFromChar(move[4]));
		else
		{
			const auto &state = currState();

			const auto srcPiece = state.boards.pieceAt(src);

			if (srcPiece == Piece::BlackKing || srcPiece == Piece::WhiteKing)
			{
				if (g_opts.chess960)
				{
					if (state.boards.pieceAt(dst) == colorPiece(BasePiece::Rook, pieceColor(srcPiece)))
						return Move::castling(src, dst);
					else return Move::standard(src, dst);
				}
				else if (std::abs(squareFile(src) - squareFile(dst)) == 2)
				{
					const auto rookFile = squareFile(src) < squareFile(dst) ? 7 : 0;
					return Move::castling(src, toSquare(squareRank(src), rookFile));
				}
			}

			if ((srcPiece == Piece::BlackPawn || srcPiece == Piece::WhitePawn)
				&& dst == state.enPassant)
				return Move::enPassant(src, dst);

			return Move::standard(src, dst);
		}
	}

	Position Position::starting()
	{
		Position position{};

		auto &state = position.currState();

		state.boards.forPiece(BasePiece::Pawn) = U64(0x00FF00000000FF00);
		state.boards.forPiece(BasePiece::Knight) = U64(0x4200000000000042);
		state.boards.forPiece(BasePiece::Bishop) = U64(0x2400000000000024);
		state.boards.forPiece(BasePiece::Rook) = U64(0x8100000000000081);
		state.boards.forPiece(BasePiece::Queen) = U64(0x0800000000000008);
		state.boards.forPiece(BasePiece::King) = U64(0x1000000000000010);

		state.boards.forColor(Color::Black) = U64(0xFFFF000000000000);
		state.boards.forColor(Color::White) = U64(0x000000000000FFFF);

		state.castlingRooks.blackShort = Square::H8;
		state.castlingRooks.blackLong  = Square::A8;
		state.castlingRooks.whiteShort = Square::H1;
		state.castlingRooks.whiteLong  = Square::A1;

		position.regen();

		return position;
	}

	std::optional<Position> Position::fromFen(const std::string &fen)
	{
		Position position{};

		const auto tokens = split::split(fen, ' ');

		if (tokens.size() > 6)
		{
			std::cerr << "excess tokens after fullmove number in fen " << fen << std::endl;
			return {};
		}

		if (tokens.size() == 5)
		{
			std::cerr << "missing fullmove number in fen " << fen << std::endl;
			return {};
		}

		if (tokens.size() == 4)
		{
			std::cerr << "missing halfmove clock in fen " << fen << std::endl;
			return {};
		}

		if (tokens.size() == 3)
		{
			std::cerr << "missing en passant square in fen " << fen << std::endl;
			return {};
		}

		if (tokens.size() == 2)
		{
			std::cerr << "missing castling availability in fen " << fen << std::endl;
			return {};
		}

		if (tokens.size() == 1)
		{
			std::cerr << "missing next move color in fen " << fen << std::endl;
			return {};
		}

		if (tokens.empty())
		{
			std::cerr << "missing ranks in fen " << fen << std::endl;
			return {};
		}

		auto &state = position.currState();

		u32 rankIdx = 0;

		const auto ranks = split::split(tokens[0], '/');
		for (const auto &rank : ranks)
		{
			if (rankIdx >= 8)
			{
				std::cerr << "too many ranks in fen " << fen << std::endl;
				return {};
			}

			u32 fileIdx = 0;

			for (const auto c : rank)
			{
				if (fileIdx >= 8)
				{
					std::cerr << "too many files in rank " << rankIdx << " in fen " << fen << std::endl;
					return {};
				}

				if (const auto emptySquares = util::tryParseDigit(c))
					fileIdx += *emptySquares;
				else if (const auto piece = pieceFromChar(c); piece != Piece::None)
				{
					state.boards.setPiece(toSquare(7 - rankIdx, fileIdx), piece);
					++fileIdx;
				}
				else
				{
					std::cerr << "invalid piece character " << c << " in fen " << fen << std::endl;
					return {};
				}
			}

			// last character was a digit
			if (fileIdx > 8)
			{
				std::cerr << "too many files in rank " << rankIdx << " in fen " << fen << std::endl;
				return {};
			}

			if (fileIdx < 8)
			{
				std::cerr << "not enough files in rank " << rankIdx << " in fen " << fen << std::endl;
				return {};
			}

			++rankIdx;
		}

		const auto &color = tokens[1];

		if (color.length() != 1)
		{
			std::cerr << "invalid next move color in fen " << fen << std::endl;
			return {};
		}

		switch (color[0])
		{
		case 'b': position.m_blackToMove = true; break;
		case 'w': break;
		default:
			std::cerr << "invalid next move color in fen " << fen << std::endl;
			return {};
		}

		const auto &castlingFlags = tokens[2];

		if (castlingFlags.length() > 4)
		{
			std::cerr << "invalid castling availability in fen " << fen << std::endl;
			return {};
		}

		if (castlingFlags.length() != 1 || castlingFlags[0] != '-')
		{
			if (g_opts.chess960)
			{
				for (i32 rank = 0; rank < 8; ++rank)
				{
					for (i32 file = 0; file < 8; ++file)
					{
						const auto square = toSquare(rank, file);

						if (state.boards.pieceAt(square) == Piece::BlackKing)
							state.blackKing = square;
						else if (state.boards.pieceAt(square) == Piece::WhiteKing)
							state.whiteKing = square;
					}
				}

				for (char flag : castlingFlags)
				{
					if (flag >= 'a' && flag <= 'h')
					{
						const auto file = static_cast<i32>(flag - 'a');
						const auto kingFile = squareFile(state.blackKing);

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return {};
						}

						if (file < kingFile)
							state.castlingRooks.blackLong = toSquare(7, file);
						else state.castlingRooks.blackShort = toSquare(7, file);
					}
					else if (flag >= 'A' && flag <= 'H')
					{
						const auto file = static_cast<i32>(flag - 'A');
						const auto kingFile = squareFile(state.whiteKing);

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return {};
						}

						if (file < kingFile)
							state.castlingRooks.whiteLong = toSquare(0, file);
						else state.castlingRooks.whiteShort = toSquare(0, file);
					}
					else if (flag == 'k')
					{
						for (i32 file = squareFile(state.blackKing) + 1; file < 8; ++file)
						{
							const auto square = toSquare(7, file);
							if (state.boards.pieceAt(square) == Piece::BlackRook)
							{
								state.castlingRooks.blackShort = square;
								break;
							}
						}
					}
					else if (flag == 'K')
					{
						for (i32 file = squareFile(state.whiteKing) + 1; file < 8; ++file)
						{
							const auto square = toSquare(0, file);
							if (state.boards.pieceAt(square) == Piece::WhiteRook)
							{
								state.castlingRooks.whiteShort = square;
								break;
							}
						}
					}
					else if (flag == 'q')
					{
						for (i32 file = squareFile(state.blackKing) - 1; file >= 0; --file)
						{
							const auto square = toSquare(7, file);
							if (state.boards.pieceAt(square) == Piece::BlackRook)
							{
								state.castlingRooks.blackLong = square;
								break;
							}
						}
					}
					else if (flag == 'Q')
					{
						for (i32 file = squareFile(state.whiteKing) - 1; file >= 0; --file)
						{
							const auto square = toSquare(0, file);
							if (state.boards.pieceAt(square) == Piece::WhiteRook)
							{
								state.castlingRooks.whiteLong = square;
								break;
							}
						}
					}
					else
					{
						std::cerr << "invalid castling availability in fen " << fen << std::endl;
						return {};
					}
				}
			}
			else
			{
				for (char flag : castlingFlags)
				{
					switch (flag)
					{
					case 'k': state.castlingRooks.blackShort = Square::H8; break;
					case 'q': state.castlingRooks.blackLong  = Square::A8; break;
					case 'K': state.castlingRooks.whiteShort = Square::H1; break;
					case 'Q': state.castlingRooks.whiteLong  = Square::A1; break;
					default:
						std::cerr << "invalid castling availability in fen " << fen << std::endl;
						return {};
					}
				}
			}
		}

		const auto &enPassant = tokens[3];

		if (enPassant != "-")
		{
			if (state.enPassant = squareFromString(enPassant); state.enPassant == Square::None)
			{
				std::cerr << "invalid en passant square in fen " << fen << std::endl;
				return {};
			}
		}

		const auto &halfmoveStr = tokens[4];

		if (const auto halfmove = util::tryParseU32(halfmoveStr))
			state.halfmove = *halfmove;
		else
		{
			std::cerr << "invalid halfmove clock in fen " << fen << std::endl;
			return {};
		}

		const auto &fullmoveStr = tokens[5];

		if (const auto fullmove = util::tryParseU32(fullmoveStr))
			position.m_fullmove = *fullmove;
		else
		{
			std::cerr << "invalid fullmove number in fen " << fen << std::endl;
			return {};
		}

		position.regen();

		return position;
	}

	Square squareFromString(const std::string &str)
	{
		if (str.length() != 2)
			return Square::None;

		char file = str[0];
		char rank = str[1];

		if (file < 'a' || file > 'h'
			|| rank < '1' || rank > '8')
			return Square::None;

		return toSquare(static_cast<u32>(rank - '1'), static_cast<u32>(file - 'a'));
	}
}
