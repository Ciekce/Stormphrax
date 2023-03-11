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

namespace polaris
{
	namespace
	{
#ifndef NDEBUG
		constexpr bool VerifyAll = true;
#endif

		constexpr std::array PhaseInc{0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0, 0};
		constexpr std::array PhaseIncBase{0, 1, 1, 2, 4, 0, 0};
	}

	HistoryGuard::~HistoryGuard()
	{
		m_pos.popMove();
	}

	template void Position::applyMoveUnchecked<false, false>(Move move);
	template void Position::applyMoveUnchecked<true, false>(Move move);
	template void Position::applyMoveUnchecked<false, true>(Move move);
	template void Position::applyMoveUnchecked<true, true>(Move move);

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
	template bool Position::verify<false>();
	template bool Position::verify<true>();
#endif

	Position::Position(bool init)
	{
		if (init)
		{
			m_history.reserve(256);

			for (auto &file: m_pieces)
			{
				file.fill(Piece::None);
			}
		}
	}

	template <bool UpdateMaterial, bool History>
	void Position::applyMoveUnchecked(Move move)
	{
		const auto prevKey = m_key;
		const auto prevPawnKey = m_pawnKey;
		const auto prevFlags = m_flags;
		const auto prevEnPassant = m_enPassant;

		m_flags ^= PositionFlags::BlackToMove;

		m_key ^= hash::color();
		m_pawnKey ^= hash::color();

		if (m_enPassant != Square::None)
		{
			m_key ^= hash::enPassant(m_enPassant);
			m_enPassant = Square::None;
		}

		if (!move)
		{
			if constexpr(History)
				m_history.push_back({prevKey, prevPawnKey, m_material, m_checkers, move, prevFlags,
					static_cast<u16>(m_halfmove), Piece::None, prevEnPassant});

#ifndef NDEBUG
			if constexpr(VerifyAll)
			{
				if (!verify<UpdateMaterial>())
				{
					printHistory(move);
					__builtin_trap();
				}
			}
#endif

			return;
		}

		const auto prevMaterial = m_material;
		const auto prevCheckers = m_checkers;
		const auto prevHalfmove = m_halfmove;

		const auto moveType = move.type();

		const auto moveSrc = move.src();
		const auto moveDst = move.dst();

		const auto currColor = opponent();

		if (currColor == Color::Black)
			++m_fullmove;

		auto castlingMask = PositionFlags::None;

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
			if (moveSrc == Square::A8)
				castlingMask |= PositionFlags::BlackQueenside;
			else if (moveSrc == Square::H8)
				castlingMask |= PositionFlags::BlackKingside;
		}
		else if (moving == Piece::WhiteRook)
		{
			if (moveSrc == Square::A1)
				castlingMask |= PositionFlags::WhiteQueenside;
			else if (moveSrc == Square::H1)
				castlingMask |= PositionFlags::WhiteKingside;
		}
		else if (moving == Piece::BlackKing)
			castlingMask |= PositionFlags::BlackCastling;
		else if (moving == Piece::WhiteKing)
			castlingMask |= PositionFlags::WhiteCastling;
		else if (moving == Piece::BlackPawn && move.srcRank() == 6 && move.dstRank() == 4)
		{
			m_enPassant = toSquare(5, move.srcFile());
			m_key ^= hash::enPassant(m_enPassant);
		}
		else if (moving == Piece::WhitePawn && move.srcRank() == 1 && move.dstRank() == 3)
		{
			m_enPassant = toSquare(2, move.srcFile());
			m_key ^= hash::enPassant(m_enPassant);
		}

		auto captured = Piece::None;

		switch (moveType)
		{
		case MoveType::Standard: captured = movePiece<true, UpdateMaterial>(moveSrc, moveDst); break;
		case MoveType::Promotion: captured = promotePawn<true, UpdateMaterial>(moveSrc, moveDst, move.target()); break;
		case MoveType::Castling: castle(moveSrc, moveDst); break;
		case MoveType::EnPassant: captured = enPassant<true, UpdateMaterial>(moveSrc, moveDst); break;
		}

		if (captured == Piece::BlackRook)
		{
			if (moveDst == Square::A8)
				castlingMask |= PositionFlags::BlackQueenside;
			else if (moveDst == Square::H8)
				castlingMask |= PositionFlags::BlackKingside;
		}
		else if (captured == Piece::WhiteRook)
		{
			if (moveDst == Square::A1)
				castlingMask |= PositionFlags::WhiteQueenside;
			else if (moveDst == Square::H1)
				castlingMask |= PositionFlags::WhiteKingside;
		}
		else if (moveType == MoveType::Castling)
			castlingMask |= pieceColor(moving) == Color::Black
				? PositionFlags::BlackCastling
				: PositionFlags::WhiteCastling;

		if (castlingMask != PositionFlags::None)
		{
			const auto oldFlags = m_flags;
			m_flags &= ~castlingMask;

			m_key ^= hash::castling( m_flags & PositionFlags::AllCastling);
			m_key ^= hash::castling(oldFlags & PositionFlags::AllCastling);
		}

		m_checkers = calcCheckers();

		m_phase = std::clamp(m_phase, 0, 24);

		if constexpr(History)
			m_history.push_back({prevKey, prevPawnKey, prevMaterial, prevCheckers, move, prevFlags,
				static_cast<u16>(prevHalfmove), captured, prevEnPassant});

#ifndef NDEBUG
		if constexpr(VerifyAll)
		{
			if (!verify<UpdateMaterial>())
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
		if (m_history.empty())
		{
			std::cerr << "popMove() with no previous move?" << std::endl;
			return;
		}
#endif

		const auto &prevMove = m_history.back();
		m_history.pop_back();

		const auto move = prevMove.move;

		m_key = prevMove.key;
		m_pawnKey = prevMove.pawnKey;
		m_flags = prevMove.flags;
		m_enPassant = prevMove.enPassant;

		if (!move)
			return;

		m_material = prevMove.material;
		m_checkers = prevMove.checkers;
		m_halfmove = prevMove.halfmove;

#if 0
		const auto moveType = move.type();

		const auto currColor = opponent();

		if (currColor == Color::Black)
			--m_fullmove;

		auto &srcSlot = pieceAt(move.src());
		auto &dstSlot = pieceAt(move.dst());

		auto &playerBoard = colorBoard(currColor);
		playerBoard ^= squareMask(move.src()) | squareMask(move.dst());

		const auto moving = moveType == MoveType::Promotion
			? colorPiece(BasePiece::Pawn, currColor)
			: dstSlot;

		dstSlot = prevMove.captured;

		auto &movingBoard = board(moving);

		if (moveType == MoveType::Promotion)
		{
			srcSlot = moving;
			movingBoard[move.src()] = true;
		}
		else
		{
			srcSlot = moving;
			movingBoard ^= squareMask(move.src()) | squareMask(move.dst());
		}

		if (prevMove.captured != Piece::None)
		{
			auto captureRank = move.dstRank();

			if (moveType == MoveType::EnPassant)
			{
				pieceAt(move.dst()) = Piece::None;
				captureRank += moving == Piece::BlackPawn ? 1 : -1;
				m_enPassant = move.dst();
			}

			const auto captureSquare = toSquare(captureRank, move.dstFile());

			pieceAt(captureSquare) = prevMove.captured;

			auto &capturedBoard = board(prevMove.captured);
			capturedBoard[captureSquare] = true;

			auto &opponentBoard = colorBoard(currColor);
			opponentBoard[captureSquare] = false;

			m_material += eval::pieceValue(prevMove.captured, captureSquare);
		}
		else if (moveType == MoveType::Castling)
		{
			const auto rank = move.srcRank();

			const auto rook = rank == 0 ? Piece::WhiteRook : Piece::BlackRook;
			auto &rookBoard = board(rook);

			Square srcSquare, dstSquare;

			// kingside
			if (move.dstFile() == 6)
			{
				srcSquare = toSquare(rank, 7);
				dstSquare = toSquare(rank, 5);

				const auto rookMask = rank == 0 ? UINT16_C(0x00000000000000A0) : UINT16_C(0xA000000000000000);

				rookBoard ^= rookMask;
				playerBoard ^= rookMask;
			}
			else // queenside
			{
				srcSquare = toSquare(rank, 0);
				dstSquare = toSquare(rank, 3);

				const auto rookMask = rank == 0 ? UINT16_C(0x0000000000000009) : UINT16_C(0x0900000000000000);

				rookBoard ^= rookMask;
				playerBoard ^= rookMask;
			}

			pieceAt(srcSquare) = rook;
			pieceAt(dstSquare) = Piece::None;
		}
#endif

		const auto moveType = move.type();

		const auto moveSrc = move.src();
		const auto moveDst = move.dst();

		const auto currColor = toMove();

		if (currColor == Color::Black)
			--m_fullmove;

		switch (moveType)
		{
		case MoveType::Standard:
			movePiece<false, false>(moveDst, moveSrc);
			if (prevMove.captured != Piece::None)
				setPiece<false, false>(moveDst, prevMove.captured);
			break;
		case MoveType::Promotion: unpromotePawn(moveSrc, moveDst, prevMove.captured); break;
		case MoveType::Castling: uncastle(moveSrc, moveDst); break;
		case MoveType::EnPassant: undoEnPassant(moveSrc, moveDst); break;
		}

#ifndef NDEBUG
		if constexpr(VerifyAll)
		{
			if (!verify())
			{
				printHistory(move);
				__builtin_trap();
			}
		}
#endif
	}

	bool Position::isPseudolegal(Move move) const
	{
		if (move.type() != MoveType::Standard)
		{
			//TODO
			MoveList moves{};
			generateAll(moves, *this);

			return std::ranges::any_of(moves.begin(), moves.end(),
				[move](const auto m) { return m == move; });
		}

		const auto us = toMove();

		const auto src = move.src();
		const auto srcPiece = pieceAt(src);

		if (pieceColor(srcPiece) != us)
			return false;

		const auto dst = move.dst();
		const auto dstPiece = pieceAt(dst);

		if (pieceColor(dstPiece) == us)
			return false;

		if (basePiece(dstPiece) == BasePiece::King)
			return false;

		const auto them = oppColor(us);
		const auto base = basePiece(srcPiece);

		const auto occ = m_blackPop | m_whitePop;

		if (base == BasePiece::Pawn)
		{
			if (!(attacks::getPawnAttacks(src, us)
				& (occupancy(them) | squareBitChecked(m_enPassant)))[dst])
				return false;

			if (move.srcFile() != move.dstFile())
				return false;

			const auto srcRank = move.srcRank();
			const auto dstRank = move.dstRank();

			i32 delta, maxDelta;

			if (us == Color::Black)
			{
				delta = srcRank - dstRank;
				maxDelta = srcRank == 6 ? 2 : 1;
			}
			else
			{
				delta = dstRank - srcRank;
				maxDelta = srcRank == 1 ? 2 : 1;
			}

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
				else
					fen << pieceToChar(pieceAt(rank, file));
			}

			if (rank > 0)
				fen << '/';
		}

		fen << (toMove() == Color::White ? " w " : " b ");

		if (castling() == PositionFlags::None)
			fen << '-';
		else
		{
			if (testFlags(m_flags, PositionFlags::WhiteKingside))
				fen << 'K';

			if (testFlags(m_flags,  PositionFlags::WhiteQueenside))
				fen << 'Q';

			if (testFlags(m_flags,  PositionFlags::BlackKingside))
				fen << 'k';

			if (testFlags(m_flags,  PositionFlags::BlackQueenside))
				fen << 'q';
		}

		if (m_enPassant != Square::None)
			fen << ' ' << squareToString(m_enPassant);
		else fen << " -";

		fen << ' ' << m_halfmove;
		fen << ' ' << m_fullmove;

		return fen.str();
	}

	void Position::clearHistory()
	{
		m_history.clear();
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::setPiece(Square square, Piece piece)
	{
		auto &slot = pieceAt(square);
		const auto captured = slot;

		if (captured != Piece::None)
		{
			board(captured)[square] = false;
			occupancy(pieceColor(captured))[square] = false;

			m_phase -= PhaseInc[static_cast<i32>(captured)];

			if constexpr(UpdateMaterial)
				m_material -= eval::pieceSquareValue(captured, square);

			if constexpr(UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, square);
				m_key ^= hash;

				if (basePiece(captured) == BasePiece::Pawn)
					m_pawnKey ^= hash;
			}
		}

		slot = piece;

		board(piece)[square] = true;
		occupancy(pieceColor(piece))[square] = true;

		if (piece == Piece::BlackKing)
			m_blackKing = square;
		else if (piece == Piece::WhiteKing)
			m_whiteKing = square;

		m_phase += PhaseInc[static_cast<size_t>(piece)];

		if constexpr(UpdateMaterial)
			m_material += eval::pieceSquareValue(piece, square);

		if constexpr(UpdateKey)
			m_key ^= hash::pieceSquare(piece, square);

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::removePiece(Square square)
	{
		auto &slot = pieceAt(square);

		const auto piece = slot;

		if (piece != Piece::None)
		{
			slot = Piece::None;

			board(piece)[square] = false;
			occupancy(pieceColor(piece))[square] = false;

			m_phase -= PhaseInc[static_cast<size_t>(piece)];

			if constexpr(UpdateMaterial)
				m_material -= eval::pieceSquareValue(piece, square);

			if constexpr(UpdateKey)
			{
				const auto hash = hash::pieceSquare(piece, square);
				m_key ^= hash;
				if (basePiece(piece) == BasePiece::Pawn)
					m_pawnKey ^= hash;
			}
		}

		return piece;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::movePiece(Square src, Square dst)
	{
		auto &srcSlot = pieceAt(src);
		auto &dstSlot = pieceAt(dst);

		const auto piece = srcSlot;

		const auto captured = dstSlot;

		if (captured != Piece::None)
		{
			board(captured)[dst] = false;
			occupancy(pieceColor(captured))[dst] = false;

			m_phase -= PhaseInc[static_cast<size_t>(captured)];

			if constexpr(UpdateMaterial)
				m_material -= eval::pieceSquareValue(captured, dst);

			if constexpr(UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, dst);
				m_key ^= hash;
				if (basePiece(captured) == BasePiece::Pawn)
					m_pawnKey ^= hash;
			}
		}

		srcSlot = Piece::None;
		dstSlot = piece;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

		board(piece) ^= mask;
		occupancy(pieceColor(piece)) ^= mask;

		if (piece == Piece::BlackKing)
			m_blackKing = dst;
		else if (piece == Piece::WhiteKing)
			m_whiteKing = dst;

		if constexpr(UpdateMaterial)
			m_material += eval::pieceSquareValue(piece, dst) - eval::pieceSquareValue(piece, src);

		if constexpr(UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, src) ^ hash::pieceSquare(piece, dst);
			m_key ^= hash;
			if (basePiece(piece) == BasePiece::Pawn)
				m_pawnKey ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::promotePawn(Square src, Square dst, BasePiece target)
	{
		auto &srcSlot = pieceAt(src);
		auto &dstSlot = pieceAt(dst);

		const auto captured = dstSlot;

		if (captured != Piece::None)
		{
			board(captured)[dst] = false;
			occupancy(pieceColor(captured))[dst] = false;

			m_phase -= PhaseInc[static_cast<size_t>(captured)];

			if constexpr(UpdateMaterial)
				m_material -= eval::pieceSquareValue(captured, dst);

			// cannot capture a pawn when promoting
			if constexpr(UpdateKey)
				m_key ^= hash::pieceSquare(captured, dst);
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

		if constexpr(UpdateMaterial)
			m_material += eval::pieceSquareValue(coloredTarget, dst)
				- eval::pieceSquareValue(pawn, src);

		if constexpr(UpdateKey)
		{
			const auto pawnHash = hash::pieceSquare(pawn, src);
			m_key ^= pawnHash ^ hash::pieceSquare(coloredTarget, dst);
			m_pawnKey ^= pawnHash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateMaterial>
	void Position::castle(Square src, Square dst)
	{
		movePiece<UpdateKey, UpdateMaterial>(src, dst);

		if (pieceAt(dst) == Piece::BlackKing)
			m_blackKing = dst;
		else m_whiteKing = dst;

		// castling cannot affect check

		u32 rank = squareRank(dst);
		u32 file = squareFile(dst);

		Square rookSrc, rookDst;

		if (file == 6)
		{
			rookSrc = toSquare(rank, 7);
			rookDst = toSquare(rank, 5);
		}
		else // queenside
		{
			rookSrc = toSquare(rank, 0);
			rookDst = toSquare(rank, 3);
		}

		movePiece<UpdateKey, UpdateMaterial>(rookSrc, rookDst);
	}

	template <bool UpdateKey, bool UpdateMaterial>
	Piece Position::enPassant(Square src, Square dst)
	{
		auto &srcSlot = pieceAt(src);
		auto &dstSlot = pieceAt(dst);

		const auto pawn = srcSlot;
		const auto color = pieceColor(pawn);

		srcSlot = Piece::None;
		dstSlot = pawn;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

		board(pawn) ^= mask;
		occupancy(color) ^= mask;

		if constexpr(UpdateMaterial)
			m_material += eval::pieceSquareValue(pawn, dst)
				- eval::pieceSquareValue(pawn, src);

		if constexpr(UpdateKey)
		{
			const auto hash = hash::pieceSquare(pawn, src) ^ hash::pieceSquare(pawn, dst);
			m_key ^= hash;
			m_pawnKey ^= hash;
		}

		u32 rank = squareRank(dst);
		u32 file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto pawnSquare = toSquare(rank, file);
		auto &pawnSlot = pieceAt(pawnSquare);

		const auto enemyPawn = pawnSlot;

		pawnSlot = Piece::None;

		board(enemyPawn)[pawnSquare] = false;
		occupancy(pieceColor(enemyPawn))[pawnSquare] = false;

		// pawns do not affect game phase

		if constexpr(UpdateMaterial)
			m_material -= eval::pieceSquareValue(enemyPawn, pawnSquare);

		if constexpr(UpdateKey)
		{
			const auto hash = hash::pieceSquare(enemyPawn, pawnSquare);
			m_key ^= hash;// ^ hash::enPassant(file);
			m_pawnKey ^= hash;
		}

		return enemyPawn;
	}

	void Position::unpromotePawn(Square src, Square dst, Piece captured)
	{
		auto &srcSlot = pieceAt(src);
		auto &dstSlot = pieceAt(dst);

		if (captured != Piece::None)
		{
			board(captured)[dst] = true;
			occupancy(pieceColor(captured))[dst] = true;

			m_phase += PhaseInc[static_cast<size_t>(captured)];
		}

		const auto target = dstSlot;
		const auto color = pieceColor(target);

		const auto pawn = colorPiece(BasePiece::Pawn, color);

		srcSlot = pawn;
		dstSlot = captured;

		board(pawn)[src] = true;
		board(target)[dst] = false;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);
		occupancy(color) ^= mask;
	}

	void Position::uncastle(Square src, Square dst)
	{
		movePiece<false, false>(dst, src);

		if (pieceAt(src) == Piece::BlackKing)
			m_blackKing = src;
		else m_whiteKing = src;

		u32 rank = squareRank(dst);
		u32 file = squareFile(dst);

		Square rookSrc, rookDst;

		if (file == 6)
		{
			rookSrc = toSquare(rank, 7);
			rookDst = toSquare(rank, 5);
		}
		else // queenside
		{
			rookSrc = toSquare(rank, 0);
			rookDst = toSquare(rank, 3);
		}

		movePiece<false, false>(rookDst, rookSrc);
	}

	void Position::undoEnPassant(Square src, Square dst)
	{
		auto &srcSlot = pieceAt(src);
		auto &dstSlot = pieceAt(dst);

		const auto pawn = dstSlot;
		const auto color = pieceColor(pawn);

		srcSlot = pawn;
		dstSlot = Piece::None;

		const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

		board(pawn) ^= mask;
		occupancy(color) ^= mask;

		u32 rank = squareRank(dst);
		u32 file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto opp = oppColor(color);

		const auto square = toSquare(rank, file);
		const auto enemyPawn = colorPiece(BasePiece::Pawn, opp);

		pieceAt(square) = enemyPawn;

		board(enemyPawn)[square] = true;
		occupancy(opp)[square] = true;

		m_enPassant = dst;
	}

	void Position::regenMaterial()
	{
		m_material = TaperedScore{};

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				if (const auto piece = m_pieces[rank][file]; piece != Piece::None)
				{
					const auto square = toSquare(rank, file);
					m_material += eval::pieceSquareValue(piece, square);
				}
			}
		}
	}

	template <bool EnPassantFromMoves>
	void Position::regen()
	{
		for (auto &board : m_boards)
		{
			board.clear();
		}

		m_phase = 0;
	//	m_material = {0, 0};
		m_key = 0;
		m_pawnKey = 0;

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				if (const auto piece = m_pieces[rank][file]; piece != Piece::None)
				{
					const auto square = toSquare(rank, file);

					board(piece)[square] = true;

					if (piece == Piece::BlackKing)
						m_blackKing = square;
					else if (piece == Piece::WhiteKing)
						m_whiteKing = square;

					m_phase += PhaseInc[static_cast<i32>(piece)];
				//	m_material += eval::pieceSquareValue(piece, square);

					const auto hash = hash::pieceSquare(piece, toSquare(rank, file));
					m_key ^= hash;

					if (basePiece(piece) == BasePiece::Pawn)
						m_pawnKey ^= hash;
				}
			}
		}

		//
		regenMaterial();

		if (m_phase > 24)
			m_phase = 24;

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

		if constexpr(EnPassantFromMoves)
		{
			m_enPassant = Square::None;

			if (!m_history.empty())
			{
				const auto lastMove = m_history.back().move;

				if (lastMove && lastMove.type() == MoveType::Standard)
				{
					const auto piece = pieceAt(lastMove.dst());

					if (basePiece(piece) == BasePiece::Pawn
						&& std::abs(lastMove.srcRank() - lastMove.dstRank()) == 2)
					{
						m_enPassant = toSquare(lastMove.dstRank()
							+ (piece == Piece::BlackPawn ? 1 : -1), lastMove.dstFile());
					}
				}
			}
		}

		const auto colorHash = hash::color(toMove());
		m_key ^= colorHash;
		m_pawnKey ^= colorHash;

		m_key ^= hash::castling(castling());
		m_key ^= hash::enPassant(m_enPassant);
	}

#ifndef NDEBUG
	void Position::printHistory(Move last)
	{
		for (size_t i = 0; i < m_history.size(); ++i)
		{
			if (i != 0)
				std::cerr << ' ';
			std::cerr << uci::moveAndTypeToString(m_history[i].move);
		}

		if (last)
		{
			if (!m_history.empty())
				std::cerr << ' ';
			std::cerr << uci::moveAndTypeToString(last);
		}

		std::cerr << std::endl;
	}

	template <bool CheckMaterial>
	bool Position::verify()
	{
		Position regened{*this};
		regened.regen<true>();

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

		PS_CHECK(m_key, regened.m_key, "keys")

		out << std::dec;

		if constexpr(CheckMaterial)
		{
			if (m_material != regened.m_material)
			{
				out << "info string material scores do not match";
				out << "\ninfo string current: ";
				printScore(out, m_material);
				out << "\ninfo string regened: ";
				printScore(out, regened.m_material);
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
			return Move::null();

		const auto src = squareFromString(move.substr(0, 2));
		const auto dst = squareFromString(move.substr(2, 2));

		if (move.length() == 5)
			return Move::promotion(src, dst, basePieceFromChar(move[4]));
		else
		{
			const auto srcPiece = pieceAt(src);

			if ((srcPiece == Piece::BlackKing || srcPiece == Piece::WhiteKing)
				&& std::abs(squareFile(src) - squareFile(dst)) == 2)
				return Move::castling(src, dst);

			if ((srcPiece == Piece::BlackPawn || srcPiece == Piece::WhitePawn)
				&& dst == m_enPassant)
				return Move::enPassant(src, dst);

			return Move::standard(src, dst);
		}
	}

	Position Position::starting()
	{
		Position position{};

		position.m_pieces[0][0] = position.m_pieces[0][7] = Piece::WhiteRook;
		position.m_pieces[0][1] = position.m_pieces[0][6] = Piece::WhiteKnight;
		position.m_pieces[0][2] = position.m_pieces[0][5] = Piece::WhiteBishop;

		position.m_pieces[0][3] = Piece::WhiteQueen;
		position.m_pieces[0][4] = Piece::WhiteKing;

		position.m_pieces[1].fill(Piece::WhitePawn);
		position.m_pieces[6].fill(Piece::BlackPawn);

		position.m_pieces[7][0] = position.m_pieces[7][7] = Piece::BlackRook;
		position.m_pieces[7][1] = position.m_pieces[7][6] = Piece::BlackKnight;
		position.m_pieces[7][2] = position.m_pieces[7][5] = Piece::BlackBishop;

		position.m_pieces[7][3] = Piece::BlackQueen;
		position.m_pieces[7][4] = Piece::BlackKing;

		position.m_flags = PositionFlags::AllCastling;

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
					position.pieceAt(7 - rankIdx, fileIdx) = piece;
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
		case 'b': position.m_flags |= PositionFlags::BlackToMove; break;
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
			for (char flag : castlingFlags)
			{
				switch (flag)
				{
				case 'k':
				case 'h':
					position.m_flags |= PositionFlags::BlackKingside; break;
				case 'q':
				case 'a':
					position.m_flags |= PositionFlags::BlackQueenside; break;
				case 'K':
				case 'H':
					position.m_flags |= PositionFlags::WhiteKingside; break;
				case 'Q':
				case 'A':
					position.m_flags |= PositionFlags::WhiteQueenside; break;
				default:
					std::cerr << "invalid castling availability in fen " << fen << std::endl;
					return {};
				}
			}
		}

		const auto &enPassant = tokens[3];

		if (enPassant != "-")
		{
			if (position.m_enPassant = squareFromString(enPassant); position.m_enPassant == Square::None)
			{
				std::cerr << "invalid en passant square in fen " << fen << std::endl;
				return {};
			}
		}

		const auto &halfmoveStr = tokens[4];

		if (const auto halfmove = util::tryParseU32(halfmoveStr))
			position.m_halfmove = *halfmove;
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
