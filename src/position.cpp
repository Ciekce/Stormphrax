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
#include "uci.h"
#include "pretty.h"
#endif
#include <algorithm>
#include <sstream>

#include "hash.h"
#include "eval/material.h"
#include "util/parse.h"
#include "util/split.h"
#include "attacks/attacks.h"
#include "movegen.h"
#include "opts.h"

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
		if (init)
		{
			m_states.reserve(256);
			m_states.push_back({});

			for (auto &file: currState().pieces)
			{
				file.fill(Piece::None);
			}
		}
	}

	template <bool UpdateMaterial, bool History>
	void Position::applyMoveUnchecked(Move move, TTable *prefetchTt)
	{
		auto &prevState = currState();

		prevState.lastMove = move;

		if constexpr (History)
			m_states.push_back(currState());

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

		const auto moving = pieceAt(moveSrc);

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
			state.key ^= hash::castling( state.castlingRooks);

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

		m_blackPop = board(Piece::BlackPawn)
			| board(Piece::BlackKnight)
			| board(Piece::BlackBishop)
			| board(Piece::BlackRook)
			| board(Piece::BlackQueen)
			| board(Piece::BlackKing);

		m_whitePop = board(Piece::WhitePawn)
			| board(Piece::WhiteKnight)
			| board(Piece::WhiteBishop)
			| board(Piece::WhiteRook)
			| board(Piece::WhiteQueen)
			| board(Piece::WhiteKing);

		if (toMove() == Color::Black)
			--m_fullmove;
	}

	bool Position::isPseudolegal(Move move) const
	{
		const auto us = toMove();

		const auto src = move.src();
		const auto srcPiece = pieceAt(src);

		if (srcPiece == Piece::None || pieceColor(srcPiece) != us)
			return false;

		const auto dst = move.dst();
		const auto dstPiece = pieceAt(dst);

		// we're capturing something
		if (dstPiece != Piece::None
			// we're capturing our own piece    and either not castling
			&& ((pieceColor(dstPiece) == us && (move.type() != MoveType::Castling
					// or trying to castle with a non-rook
					|| dstPiece != colorPiece(BasePiece::Rook, us)))
				// or trying to capture a king
				|| basePiece(dstPiece) == BasePiece::King))
			return false;

		if (move.type() != MoveType::Standard)
		{
			//TODO
			ScoredMoveList moves{};
			generateAll(moves, *this);

			return std::ranges::any_of(moves.begin(), moves.end(),
				[move](const auto m) { return m.move == move; });
		}

		const auto them = oppColor(us);
		const auto base = basePiece(srcPiece);

		const auto occ = m_blackPop | m_whitePop;

		if (base == BasePiece::Pawn)
		{
			const auto srcRank = move.srcRank();
			const auto dstRank = move.dstRank();

			// backwards move
			if ((us == Color::Black && dstRank >= srcRank)
				|| (us == Color::White && dstRank <= srcRank))
				return false;

			// non-promotion move to back rank
			if (dstRank == relativeRank(us, 7))
				return false;

			// sideways move
			if (move.srcFile() != move.dstFile())
			{
				// not valid attack
				if (!(attacks::getPawnAttacks(src, us)
					& (occupancy(them) | squareBitChecked(currState().enPassant)))[dst])
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
				if (pieceAt(rank, file) == Piece::None)
				{
					u32 emptySquares = 1;
					for (; file < 7 && pieceAt(rank, file + 1) == Piece::None; ++file, ++emptySquares) {}

					fen << static_cast<char>('0' + emptySquares);
				}
				else fen << pieceToChar(pieceAt(rank, file));
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
		auto &slot = pieceRefAt(square);
		const auto captured = slot;

		if (captured != Piece::None)
		{
			board(captured)[square] = false;
			occupancy(pieceColor(captured))[square] = false;

			currState().phase -= PhaseInc[static_cast<i32>(captured)];

			if constexpr (UpdateMaterial)
				currState().material -= eval::pieceSquareValue(captured, square);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, square);
				currState().key ^= hash;

				if (basePiece(captured) == BasePiece::Pawn)
					currState().pawnKey ^= hash;
			}
		}

		slot = piece;

		board(piece)[square] = true;
		occupancy(pieceColor(piece))[square] = true;

		if (piece == Piece::BlackKing)
			currState().blackKing = square;
		else if (piece == Piece::WhiteKing)
			currState().whiteKing = square;

		currState().phase += PhaseInc[static_cast<usize>(piece)];

		if constexpr (UpdateMaterial)
			currState().material += eval::pieceSquareValue(piece, square);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, square);
			currState().key ^= hash;

			if (basePiece(piece) == BasePiece::Pawn)
				currState().pawnKey ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::removePiece(Square square)
	{
		auto &slot = pieceRefAt(square);

		const auto piece = slot;

		if (piece != Piece::None)
		{
			slot = Piece::None;

			board(piece)[square] = false;
			occupancy(pieceColor(piece))[square] = false;

			currState().phase -= PhaseInc[static_cast<usize>(piece)];

			if constexpr (UpdateMaterial)
				currState().material -= eval::pieceSquareValue(piece, square);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(piece, square);
				currState().key ^= hash;
				if (basePiece(piece) == BasePiece::Pawn)
					currState().pawnKey ^= hash;
			}
		}

		return piece;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::movePiece(Square src, Square dst)
	{
		auto &srcSlot = pieceRefAt(src);
		auto &dstSlot = pieceRefAt(dst);

		const auto piece = srcSlot;

		const auto captured = dstSlot;

		if (captured != Piece::None)
		{
			board(captured)[dst] = false;
			occupancy(pieceColor(captured))[dst] = false;

			currState().phase -= PhaseInc[static_cast<usize>(captured)];

			if constexpr (UpdateMaterial)
				currState().material -= eval::pieceSquareValue(captured, dst);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, dst);
				currState().key ^= hash;
				if (basePiece(captured) == BasePiece::Pawn)
					currState().pawnKey ^= hash;
			}
		}

		srcSlot = Piece::None;
		dstSlot = piece;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

		board(piece) ^= mask;
		occupancy(pieceColor(piece)) ^= mask;

		if (piece == Piece::BlackKing)
			currState().blackKing = dst;
		else if (piece == Piece::WhiteKing)
			currState().whiteKing = dst;

		if constexpr (UpdateMaterial)
			currState().material += eval::pieceSquareValue(piece, dst) - eval::pieceSquareValue(piece, src);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, src) ^ hash::pieceSquare(piece, dst);
			currState().key ^= hash;
			if (basePiece(piece) == BasePiece::Pawn)
				currState().pawnKey ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::promotePawn(Square src, Square dst, BasePiece target)
	{
		auto &srcSlot = pieceRefAt(src);
		auto &dstSlot = pieceRefAt(dst);

		const auto captured = dstSlot;

		if (captured != Piece::None)
		{
			board(captured)[dst] = false;
			occupancy(pieceColor(captured))[dst] = false;

			currState().phase -= PhaseInc[static_cast<usize>(captured)];

			if constexpr (UpdateMaterial)
				currState().material -= eval::pieceSquareValue(captured, dst);

			// cannot capture a pawn when promoting
			if constexpr (UpdateKey)
				currState().key ^= hash::pieceSquare(captured, dst);
		}

		const auto pawn = srcSlot;
		const auto color = pieceColor(pawn);

		const auto coloredTarget = colorPiece(target, color);

		srcSlot = Piece::None;
		dstSlot = coloredTarget;

		board(pawn)[src] = false;
		board(coloredTarget)[dst] = true;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);
		occupancy(color) ^= mask;

		if constexpr (UpdateMaterial)
			currState().material += eval::pieceSquareValue(coloredTarget, dst)
				- eval::pieceSquareValue(pawn, src);

		if constexpr (UpdateKey)
		{
			const auto pawnHash = hash::pieceSquare(pawn, src);
			currState().key ^= pawnHash ^ hash::pieceSquare(coloredTarget, dst);
			currState().pawnKey ^= pawnHash;
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
		auto &srcSlot = pieceRefAt(src);
		auto &dstSlot = pieceRefAt(dst);

		const auto pawn = srcSlot;
		const auto color = pieceColor(pawn);

		srcSlot = Piece::None;
		dstSlot = pawn;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

		board(pawn) ^= mask;
		occupancy(color) ^= mask;

		if constexpr (UpdateMaterial)
			currState().material += eval::pieceSquareValue(pawn, dst)
				- eval::pieceSquareValue(pawn, src);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(pawn, src) ^ hash::pieceSquare(pawn, dst);
			currState().key ^= hash;
			currState().pawnKey ^= hash;
		}

		u32 rank = squareRank(dst);
		u32 file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto pawnSquare = toSquare(rank, file);
		auto &pawnSlot = pieceRefAt(pawnSquare);

		const auto enemyPawn = pawnSlot;

		pawnSlot = Piece::None;

		board(enemyPawn)[pawnSquare] = false;
		occupancy(pieceColor(enemyPawn))[pawnSquare] = false;

		// pawns do not affect game phase

		if constexpr (UpdateMaterial)
			currState().material -= eval::pieceSquareValue(enemyPawn, pawnSquare);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(enemyPawn, pawnSquare);
			currState().key ^= hash;// ^ hash::enPassant(file);
			currState().pawnKey ^= hash;
		}

		return enemyPawn;
	}

	void Position::regenMaterial()
	{
		currState().material = TaperedScore{};

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				if (const auto piece = currState().pieces[rank][file]; piece != Piece::None)
				{
					const auto square = toSquare(rank, file);
					currState().material += eval::pieceSquareValue(piece, square);
				}
			}
		}
	}

	template <bool EnPassantFromMoves>
	void Position::regen()
	{
		auto &state = currState();
		
		for (auto &board : state.boards)
		{
			board.clear();
		}

		state.phase = 0;
	//	state.material = {0, 0};
		state.key = 0;
		state.pawnKey = 0;

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				if (const auto piece = state.pieces[rank][file]; piece != Piece::None)
				{
					const auto square = toSquare(rank, file);

					board(piece)[square] = true;

					if (piece == Piece::BlackKing)
						state.blackKing = square;
					else if (piece == Piece::WhiteKing)
						state.whiteKing = square;

					state.phase += PhaseInc[static_cast<i32>(piece)];
				//	state.material += eval::pieceSquareValue(piece, square);

					const auto hash = hash::pieceSquare(piece, toSquare(rank, file));
					state.key ^= hash;

					if (basePiece(piece) == BasePiece::Pawn)
						state.pawnKey ^= hash;
				}
			}
		}

		//
		regenMaterial();

		if (state.phase > 24)
			state.phase = 24;

		m_blackPop = board(Piece::BlackPawn)
			| board(Piece::BlackKnight)
			| board(Piece::BlackBishop)
			| board(Piece::BlackRook)
			| board(Piece::BlackQueen)
			| board(Piece::BlackKing);

		m_whitePop = board(Piece::WhitePawn)
			| board(Piece::WhiteKnight)
			| board(Piece::WhiteBishop)
			| board(Piece::WhiteRook)
			| board(Piece::WhiteQueen)
			| board(Piece::WhiteKing);

		if constexpr (EnPassantFromMoves)
		{
			state.enPassant = Square::None;

			if (m_states.size() > 1)
			{
				const auto lastMove = m_states[m_states.size() - 2].lastMove;

				if (lastMove && lastMove.type() == MoveType::Standard)
				{
					const auto piece = pieceAt(lastMove.dst());

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
#define PS_CHECK_PIECE(P, Str) PS_CHECK(board(P), regened.board(P), Str " boards")
#define PS_CHECK_PIECES(P, Str) \
PS_CHECK_PIECE(Piece::Black ## P, "black " Str) \
PS_CHECK_PIECE(Piece::White ## P, "white " Str)

		PS_CHECK_PIECES(Pawn, "pawn")
		PS_CHECK_PIECES(Knight, "knight")
		PS_CHECK_PIECES(Bishop, "bishop")
		PS_CHECK_PIECES(Rook, "rook")
		PS_CHECK_PIECES(Queen, "queen")
		PS_CHECK_PIECES(King, "king")

		PS_CHECK(occupancy(Color::Black), regened.occupancy(Color::Black), "black occupancy boards")
		PS_CHECK(occupancy(Color::White), regened.occupancy(Color::White), "white occupancy boards")

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
			const auto srcPiece = pieceAt(src);

			if (srcPiece == Piece::BlackKing || srcPiece == Piece::WhiteKing)
			{
				if (g_opts.chess960)
				{
					if (pieceAt(dst) == colorPiece(BasePiece::Rook, pieceColor(srcPiece)))
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
				&& dst == currState().enPassant)
				return Move::enPassant(src, dst);

			return Move::standard(src, dst);
		}
	}

	Position Position::starting()
	{
		Position position{};

		auto &state = position.currState();

		state.pieces[0][0] = state.pieces[0][7] = Piece::WhiteRook;
		state.pieces[0][1] = state.pieces[0][6] = Piece::WhiteKnight;
		state.pieces[0][2] = state.pieces[0][5] = Piece::WhiteBishop;

		state.pieces[0][3] = Piece::WhiteQueen;
		state.pieces[0][4] = Piece::WhiteKing;

		state.pieces[1].fill(Piece::WhitePawn);
		state.pieces[6].fill(Piece::BlackPawn);

		state.pieces[7][0] = state.pieces[7][7] = Piece::BlackRook;
		state.pieces[7][1] = state.pieces[7][6] = Piece::BlackKnight;
		state.pieces[7][2] = state.pieces[7][5] = Piece::BlackBishop;

		state.pieces[7][3] = Piece::BlackQueen;
		state.pieces[7][4] = Piece::BlackKing;

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
					position.pieceRefAt(7 - rankIdx, fileIdx) = piece;
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

		auto &state = position.currState();

		if (castlingFlags.length() != 1 || castlingFlags[0] != '-')
		{
			if (g_opts.chess960)
			{
				for (i32 rank = 0; rank < 8; ++rank)
				{
					for (i32 file = 0; file < 8; ++file)
					{
						const auto square = toSquare(rank, file);

						if (position.pieceAt(square) == Piece::BlackKing)
							state.blackKing = square;
						else if (position.pieceAt(square) == Piece::WhiteKing)
							state.whiteKing = square;
					}
				}

				for (char flag : castlingFlags)
				{
					if (flag >= 'a' && flag <= 'h')
					{
						const auto file = static_cast<i32>(flag - 'a');
						const auto kingFile = squareFile(position.currState().blackKing);

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
						const auto kingFile = squareFile(position.currState().whiteKing);

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
							if (position.pieceAt(square) == Piece::BlackRook)
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
							if (position.pieceAt(square) == Piece::WhiteRook)
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
							if (position.pieceAt(square) == Piece::BlackRook)
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
							if (position.pieceAt(square) == Piece::WhiteRook)
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
