/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#ifndef NDEBUG
#include <iostream>
#include <iomanip>
#include "../uci.h"
#include "../pretty.h"
#endif
#include <algorithm>
#include <sstream>
#include <cassert>

#include "../hash.h"
#include "../util/parse.h"
#include "../util/split.h"
#include "../attacks/attacks.h"
#include "../movegen.h"
#include "../opts.h"
#include "../rays.h"

namespace stormphrax
{
	namespace
	{
#ifndef NDEBUG
		constexpr bool VerifyAll = true;
#endif

		auto scharnaglToBackrank(u32 n)
		{
			// https://en.wikipedia.org/wiki/Fischer_random_chess_numbering_scheme#Direct_derivation

			// these are stored with the second knight moved left by an empty square,
			// because the first knight fills a square before the second knight is placed
			static constexpr auto N5n = std::array{
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

			std::array<BasePiece, 8> dst{};
			// no need to fill with empty pieces, because pawns are impossible

			const auto placeInNthFree = [&dst](u32 n, BasePiece piece)
			{
				for (i32 free = 0, i = 0; i < 8; ++i)
				{
					if (dst[i] == BasePiece::Pawn
						&& free++ == n)
					{
						dst[i] = piece;
						break;
					}
				}
			};

			const auto placeInFirstFree = [&dst](BasePiece piece)
			{
				for (i32 i = 0; i < 8; ++i)
				{
					if (dst[i] == BasePiece::Pawn)
					{
						dst[i] = piece;
						break;
					}
				}
			};

			const auto n2 = n  / 4;
			const auto b1 = n  % 4;

			const auto n3 = n2 / 4;
			const auto b2 = n2 % 4;

			const auto n4 = n3 / 6;
			const auto  q = n3 % 6;

			dst[b1 * 2 + 1] = BasePiece::Bishop;
			dst[b2 * 2    ] = BasePiece::Bishop;

			placeInNthFree(q, BasePiece::Queen);

			const auto [knight1, knight2] = N5n[n4];

			placeInNthFree(knight1, BasePiece::Knight);
			placeInNthFree(knight2, BasePiece::Knight);

			placeInFirstFree(BasePiece::Rook);
			placeInFirstFree(BasePiece::King);
			placeInFirstFree(BasePiece::Rook);

			return dst;
		}
	}

	template auto Position::applyMoveUnchecked<false, false>(Move, eval::NnueState *, TTable *) -> bool;
	template auto Position::applyMoveUnchecked<true, false>(Move, eval::NnueState *, TTable *) -> bool;
	template auto Position::applyMoveUnchecked<false, true>(Move, eval::NnueState *, TTable *) -> bool;
	template auto Position::applyMoveUnchecked<true, true>(Move, eval::NnueState *, TTable *) -> bool;

	template auto Position::popMove<false>(eval::NnueState *) -> void;
	template auto Position::popMove<true>(eval::NnueState *) -> void;

	template auto Position::setPiece<false, false>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::setPiece<true, false>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::setPiece<false, true>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::setPiece<true, true>(Piece, Square, eval::NnueState *) -> void;

	template auto Position::removePiece<false, false>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::removePiece<true, false>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::removePiece<false, true>(Piece, Square, eval::NnueState *) -> void;
	template auto Position::removePiece<true, true>(Piece, Square, eval::NnueState *) -> void;

	template auto Position::movePieceNoCap<false, false>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::movePieceNoCap<true, false>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::movePieceNoCap<false, true>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::movePieceNoCap<true, true>(Piece, Square, Square, eval::NnueState *) -> void;

	template auto Position::movePiece<false, false>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::movePiece<true, false>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::movePiece<false, true>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::movePiece<true, true>(Piece, Square, Square, eval::NnueState *) -> Piece;

	template auto Position::promotePawn<false, false>(Piece, Square, Square, BasePiece, eval::NnueState *) -> Piece;
	template auto Position::promotePawn<true, false>(Piece, Square, Square, BasePiece, eval::NnueState *) -> Piece;
	template auto Position::promotePawn<false, true>(Piece, Square, Square, BasePiece, eval::NnueState *) -> Piece;
	template auto Position::promotePawn<true, true>(Piece, Square, Square, BasePiece, eval::NnueState *) -> Piece;

	template auto Position::castle<false, false>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::castle<true, false>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::castle<false, true>(Piece, Square, Square, eval::NnueState *) -> void;
	template auto Position::castle<true, true>(Piece, Square, Square, eval::NnueState *) -> void;

	template auto Position::enPassant<false, false>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::enPassant<true, false>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::enPassant<false, true>(Piece, Square, Square, eval::NnueState *) -> Piece;
	template auto Position::enPassant<true, true>(Piece, Square, Square, eval::NnueState *) -> Piece;

	template auto Position::regen<false>() -> void;
	template auto Position::regen<true>() -> void;

#ifndef NDEBUG
	template bool Position::verify<false>();
	template bool Position::verify<true>();
#endif

	Position::Position()
	{
		m_states.reserve(256);
		m_hashes.reserve(512);

		m_states.push_back({});
	}

	auto Position::resetToStarting() -> void
	{
		m_states.resize(1);
		m_hashes.clear();

		auto &state = currState();
		state = BoardState{};

		state.boards.forPiece(BasePiece::  Pawn) = U64(0x00FF00000000FF00);
		state.boards.forPiece(BasePiece::Knight) = U64(0x4200000000000042);
		state.boards.forPiece(BasePiece::Bishop) = U64(0x2400000000000024);
		state.boards.forPiece(BasePiece::  Rook) = U64(0x8100000000000081);
		state.boards.forPiece(BasePiece:: Queen) = U64(0x0800000000000008);
		state.boards.forPiece(BasePiece::  King) = U64(0x1000000000000010);

		state.boards.forColor(Color::Black) = U64(0xFFFF000000000000);
		state.boards.forColor(Color::White) = U64(0x000000000000FFFF);

		state.castlingRooks.shortSquares.black = Square::H8;
		state.castlingRooks.longSquares.black  = Square::A8;
		state.castlingRooks.shortSquares.white = Square::H1;
		state.castlingRooks.longSquares.white  = Square::A1;

		m_blackToMove = false;
		m_fullmove = 1;

		regen();
	}

	auto Position::resetFromFen(const std::string &fen) -> bool
	{
		const auto tokens = split::split(fen, ' ');

		if (tokens.size() > 6)
		{
			std::cerr << "excess tokens after fullmove number in fen " << fen << std::endl;
			return false;
		}

		if (tokens.size() == 5)
		{
			std::cerr << "missing fullmove number in fen " << fen << std::endl;
			return false;
		}

		if (tokens.size() == 4)
		{
			std::cerr << "missing halfmove clock in fen " << fen << std::endl;
			return false;
		}

		if (tokens.size() == 3)
		{
			std::cerr << "missing en passant square in fen " << fen << std::endl;
			return false;
		}

		if (tokens.size() == 2)
		{
			std::cerr << "missing castling availability in fen " << fen << std::endl;
			return false;
		}

		if (tokens.size() == 1)
		{
			std::cerr << "missing next move color in fen " << fen << std::endl;
			return false;
		}

		if (tokens.empty())
		{
			std::cerr << "missing ranks in fen " << fen << std::endl;
			return false;
		}

		BoardState newState{};

		u32 rankIdx = 0;

		const auto ranks = split::split(tokens[0], '/');
		for (const auto &rank : ranks)
		{
			if (rankIdx >= 8)
			{
				std::cerr << "too many ranks in fen " << fen << std::endl;
				return false;
			}

			u32 fileIdx = 0;

			for (const auto c : rank)
			{
				if (fileIdx >= 8)
				{
					std::cerr << "too many files in rank " << rankIdx << " in fen " << fen << std::endl;
					return false;
				}

				if (const auto emptySquares = util::tryParseDigit(c))
					fileIdx += *emptySquares;
				else if (const auto piece = pieceFromChar(c); piece != Piece::None)
				{
					newState.boards.setPiece(toSquare(7 - rankIdx, fileIdx), piece);
					++fileIdx;
				}
				else
				{
					std::cerr << "invalid piece character " << c << " in fen " << fen << std::endl;
					return false;
				}
			}

			// last character was a digit
			if (fileIdx > 8)
			{
				std::cerr << "too many files in rank " << rankIdx << " in fen " << fen << std::endl;
				return false;
			}

			if (fileIdx < 8)
			{
				std::cerr << "not enough files in rank " << rankIdx << " in fen " << fen << std::endl;
				return false;
			}

			++rankIdx;
		}

		if (newState.boards.forPiece(Piece::BlackKing).popcount() != 1)
		{
			std::cerr << "black must have exactly 1 king" << std::endl;
			return false;
		}

		if (newState.boards.forPiece(Piece::WhiteKing).popcount() != 1)
		{
			std::cerr << "white must have exactly 1 king" << std::endl;
			return false;
		}

		const auto &color = tokens[1];

		if (color.length() != 1)
		{
			std::cerr << "invalid next move color in fen " << fen << std::endl;
			return false;
		}

		bool newBlackToMove = false;

		switch (color[0])
		{
		case 'b': newBlackToMove = true; break;
		case 'w': break;
		default:
			std::cerr << "invalid next move color in fen " << fen << std::endl;
			return false;
		}

		if (const auto stm = newBlackToMove ? Color::Black : Color::White;
			isAttacked<false>(newState, stm,
				newState.boards.forPiece(BasePiece::King, oppColor(stm)).lowestSquare(),
				stm))
		{
			std::cerr << "opponent must not be in check" << std::endl;
			return false;
		}

		const auto &castlingFlags = tokens[2];

		if (castlingFlags.length() > 4)
		{
			std::cerr << "invalid castling availability in fen " << fen << std::endl;
			return false;
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

						const auto piece = newState.boards.pieceAt(square);
						if (basePiece(piece) == BasePiece::King)
							newState.king(pieceColor(piece)) = square;
					}
				}

				for (char flag : castlingFlags)
				{
					if (flag >= 'a' && flag <= 'h')
					{
						const auto file = static_cast<i32>(flag - 'a');
						const auto kingFile = squareFile(newState.blackKing());

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return false;
						}

						if (file < kingFile)
							newState.castlingRooks.longSquares.black = toSquare(7, file);
						else newState.castlingRooks.shortSquares.black = toSquare(7, file);
					}
					else if (flag >= 'A' && flag <= 'H')
					{
						const auto file = static_cast<i32>(flag - 'A');
						const auto kingFile = squareFile(newState.whiteKing());

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return false;
						}

						if (file < kingFile)
							newState.castlingRooks.longSquares.white = toSquare(0, file);
						else newState.castlingRooks.shortSquares.white = toSquare(0, file);
					}
					else if (flag == 'k')
					{
						for (i32 file = squareFile(newState.blackKing()) + 1; file < 8; ++file)
						{
							const auto square = toSquare(7, file);
							if (newState.boards.pieceAt(square) == Piece::BlackRook)
							{
								newState.castlingRooks.shortSquares.black = square;
								break;
							}
						}
					}
					else if (flag == 'K')
					{
						for (i32 file = squareFile(newState.whiteKing()) + 1; file < 8; ++file)
						{
							const auto square = toSquare(0, file);
							if (newState.boards.pieceAt(square) == Piece::WhiteRook)
							{
								newState.castlingRooks.shortSquares.white = square;
								break;
							}
						}
					}
					else if (flag == 'q')
					{
						for (i32 file = squareFile(newState.blackKing()) - 1; file >= 0; --file)
						{
							const auto square = toSquare(7, file);
							if (newState.boards.pieceAt(square) == Piece::BlackRook)
							{
								newState.castlingRooks.longSquares.black = square;
								break;
							}
						}
					}
					else if (flag == 'Q')
					{
						for (i32 file = squareFile(newState.whiteKing()) - 1; file >= 0; --file)
						{
							const auto square = toSquare(0, file);
							if (newState.boards.pieceAt(square) == Piece::WhiteRook)
							{
								newState.castlingRooks.longSquares.white = square;
								break;
							}
						}
					}
					else
					{
						std::cerr << "invalid castling availability in fen " << fen << std::endl;
						return false;
					}
				}
			}
			else
			{
				for (char flag : castlingFlags)
				{
					switch (flag)
					{
					case 'k': newState.castlingRooks.shortSquares.black = Square::H8; break;
					case 'q': newState.castlingRooks.longSquares.black  = Square::A8; break;
					case 'K': newState.castlingRooks.shortSquares.white = Square::H1; break;
					case 'Q': newState.castlingRooks.longSquares.white  = Square::A1; break;
					default:
						std::cerr << "invalid castling availability in fen " << fen << std::endl;
						return false;
					}
				}
			}
		}

		const auto &enPassant = tokens[3];

		if (enPassant != "-")
		{
			if (newState.enPassant = squareFromString(enPassant); newState.enPassant == Square::None)
			{
				std::cerr << "invalid en passant square in fen " << fen << std::endl;
				return false;
			}
		}

		const auto &halfmoveStr = tokens[4];

		if (const auto halfmove = util::tryParseU32(halfmoveStr))
			newState.halfmove = *halfmove;
		else
		{
			std::cerr << "invalid halfmove clock in fen " << fen << std::endl;
			return false;
		}

		const auto &fullmoveStr = tokens[5];

		u32 newFullmove;

		if (const auto fullmove = util::tryParseU32(fullmoveStr))
			newFullmove = *fullmove;
		else
		{
			std::cerr << "invalid fullmove number in fen " << fen << std::endl;
			return false;
		}

		m_states.resize(1);
		m_hashes.clear();

		m_blackToMove = newBlackToMove;
		m_fullmove = newFullmove;

		currState() = newState;

		regen();

		return true;
	}

	auto Position::resetFromFrcIndex(u32 n) -> bool
	{
		assert(g_opts.chess960);

		if (n >= 960)
		{
			std::cerr << "invalid frc position index " << n << std::endl;
			return false;
		}

		m_states.resize(1);
		m_hashes.clear();

		auto &state = currState();
		state = BoardState{};

		state.boards.forPiece(BasePiece::Pawn) = U64(0x00FF00000000FF00);

		state.boards.forColor(Color::Black) = U64(0x00FF000000000000);
		state.boards.forColor(Color::White) = U64(0x000000000000FF00);

		const auto backrank = scharnaglToBackrank(n);

		bool firstRook = true;

		for (i32 i = 0; i < 8; ++i)
		{
			const auto blackSquare = toSquare(7, i);
			const auto whiteSquare = toSquare(0, i);

			state.boards.setPiece(blackSquare, colorPiece(backrank[i], Color::Black));
			state.boards.setPiece(whiteSquare, colorPiece(backrank[i], Color::White));

			if (backrank[i] == BasePiece::Rook)
			{
				auto &rookPair = firstRook
					? state.castlingRooks. longSquares
					: state.castlingRooks.shortSquares;

				rookPair.black = blackSquare;
				rookPair.white = whiteSquare;

				firstRook = false;
			}
		}

		m_blackToMove = false;
		m_fullmove = 1;

		regen();

		return true;
	}

	auto Position::resetFromDfrcIndex(u32 n) -> bool
	{
		assert(g_opts.chess960);

		if (n >= 960 * 960)
		{
			std::cerr << "invalid dfrc position index " << n << std::endl;
			return false;
		}

		m_states.resize(1);
		m_hashes.clear();

		auto &state = currState();
		state = BoardState{};

		state.boards.forPiece(BasePiece::Pawn) = U64(0x00FF00000000FF00);

		state.boards.forColor(Color::Black) = U64(0x00FF000000000000);
		state.boards.forColor(Color::White) = U64(0x000000000000FF00);

		const auto blackBackrank = scharnaglToBackrank(n / 960);
		const auto whiteBackrank = scharnaglToBackrank(n % 960);

		bool firstBlackRook = true;
		bool firstWhiteRook = true;

		for (i32 i = 0; i < 8; ++i)
		{
			const auto blackSquare = toSquare(7, i);
			const auto whiteSquare = toSquare(0, i);

			state.boards.setPiece(blackSquare, colorPiece(blackBackrank[i], Color::Black));
			state.boards.setPiece(whiteSquare, colorPiece(whiteBackrank[i], Color::White));

			if (blackBackrank[i] == BasePiece::Rook)
			{
				auto &rookPair = firstBlackRook
					? state.castlingRooks. longSquares
					: state.castlingRooks.shortSquares;

				rookPair.black = blackSquare;

				firstBlackRook = false;
			}

			if (whiteBackrank[i] == BasePiece::Rook)
			{
				auto &rookPair = firstWhiteRook
					? state.castlingRooks. longSquares
					: state.castlingRooks.shortSquares;

				rookPair.white = whiteSquare;

				firstWhiteRook = false;
			}
		}

		m_blackToMove = false;
		m_fullmove = 1;

		regen();

		return true;
	}

	template <bool UpdateNnue, bool StateHistory>
	auto Position::applyMoveUnchecked(Move move, eval::NnueState *nnueState, TTable *prefetchTt) -> bool
	{
		if constexpr (UpdateNnue && StateHistory)
			nnueState->push();

		auto &prevState = currState();

		prevState.lastMove = move;

		if constexpr (StateHistory)
		{
			assert(m_states.size() < m_states.capacity());
			m_states.push_back(prevState);
		}

		m_hashes.push_back(prevState.key);

		auto &state = currState();

		m_blackToMove = !m_blackToMove;

		state.key ^= hash::color();

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
				if (!verify<StateHistory>())
				{
					printHistory(move);
					__builtin_trap();
				}
			}
#endif

			state.threats = calcThreats();

			return true;
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

		auto captured = Piece::None;

		switch (moveType)
		{
		case MoveType::Standard:
			captured = movePiece<true, UpdateNnue>(moving, moveSrc, moveDst, nnueState);
			break;
		case MoveType::Promotion:
			captured = promotePawn<true, UpdateNnue>(moving, moveSrc, moveDst, move.target(), nnueState);
			break;
		case MoveType::Castling:
			castle<true, UpdateNnue>(moving, moveSrc, moveDst, nnueState);
			break;
		case MoveType::EnPassant:
			captured = enPassant<true, UpdateNnue>(moving, moveSrc, moveDst, nnueState);
			break;
		}

		if (isAttacked<false>(state, toMove(), state.king(currColor), toMove()))
			return false;

		if (moving == Piece::BlackRook)
		{
			if (moveSrc == state.castlingRooks.shortSquares.black)
				newCastlingRooks.shortSquares.black = Square::None;
			if (moveSrc == state.castlingRooks.longSquares.black)
				newCastlingRooks.longSquares.black = Square::None;
		}
		else if (moving == Piece::WhiteRook)
		{
			if (moveSrc == state.castlingRooks.shortSquares.white)
				newCastlingRooks.shortSquares.white = Square::None;
			if (moveSrc == state.castlingRooks.longSquares.white)
				newCastlingRooks.longSquares.white = Square::None;
		}
		else if (moving == Piece::BlackKing)
			newCastlingRooks.shortSquares.black = newCastlingRooks.longSquares.black = Square::None;
		else if (moving == Piece::WhiteKing)
			newCastlingRooks.shortSquares.white = newCastlingRooks.longSquares.white = Square::None;
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

		if (captured == Piece::None
			&& basePiece(moving) != BasePiece::Pawn)
			++state.halfmove;
		else state.halfmove = 0;

		if (captured == Piece::BlackRook)
		{
			if (moveDst == state.castlingRooks.shortSquares.black)
				newCastlingRooks.shortSquares.black = Square::None;
			if (moveDst == state.castlingRooks.longSquares.black)
				newCastlingRooks.longSquares.black = Square::None;
		}
		else if (captured == Piece::WhiteRook)
		{
			if (moveDst == state.castlingRooks.shortSquares.white)
				newCastlingRooks.shortSquares.white = Square::None;
			if (moveDst == state.castlingRooks.longSquares.white)
				newCastlingRooks.longSquares.white = Square::None;
		}
		else if (moveType == MoveType::Castling)
		{
			if (pieceColor(moving) == Color::Black)
				newCastlingRooks.shortSquares.black = newCastlingRooks.longSquares.black = Square::None;
			else newCastlingRooks.shortSquares.white = newCastlingRooks.longSquares.white = Square::None;
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
		state.threats = calcThreats();

#ifndef NDEBUG
		if constexpr (VerifyAll)
		{
			if (!verify<StateHistory>())
			{
				printHistory();
				__builtin_trap();
			}
		}
#endif

		return true;
	}

	template <bool UpdateNnue>
	auto Position::popMove(eval::NnueState *nnueState) -> void
	{
		assert(m_states.size() > 1 && "popMove() with no previous move?");

		if constexpr (UpdateNnue)
			nnueState->pop();

		m_states.pop_back();
		m_hashes.pop_back();

		m_blackToMove = !m_blackToMove;

		if (!currState().lastMove)
			return;

		if (toMove() == Color::Black)
			--m_fullmove;
	}

	auto Position::clearStateHistory() -> void
	{
		const auto state = currState();
		m_states.resize(1);
		currState() = state;
	}

	auto Position::isPseudolegal(Move move) const -> bool
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
				if (dst != (us == Color::Black
						? state.castlingRooks.shortSquares.black
						: state.castlingRooks.shortSquares.white))
					return false;

				kingDst = toSquare(rank, 6);
				rookDst = toSquare(rank, 5);
			}
			else
			{
				// no castling rights
				if (dst != (us == Color::Black
						? state.castlingRooks.longSquares.black
						: state.castlingRooks.longSquares.white))
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
				if (dst == state.castlingRooks.shortSquares.black)
					return (occ & U64(0x6000000000000000)).empty()
						&& !isAttacked(Square::F8, Color::White);
				else if (dst == state.castlingRooks.longSquares.black)
					return (occ & U64(0x0E00000000000000)).empty()
						&& !isAttacked(Square::D8, Color::White);
				else if (dst == state.castlingRooks.shortSquares.white)
					return (occ & U64(0x0000000000000060)).empty()
						&& !isAttacked(Square::F1, Color::Black);
				else return (occ & U64(0x000000000000000E)).empty()
						&& !isAttacked(Square::D1, Color::Black);
			}
		}

		if (base == BasePiece::Pawn)
		{
			if (type == MoveType::EnPassant)
				return dst == state.enPassant && attacks::getPawnAttacks(state.enPassant, them)[src];

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
				if (!(attacks::getPawnAttacks(src, us) & state.boards.forColor(them))[dst])
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

	auto Position::toFen() const -> std::string
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

			if (state.castlingRooks.shortSquares.white != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.shortSquares.white)];
			if (state.castlingRooks.longSquares.white != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.longSquares.white)];
			if (state.castlingRooks.shortSquares.black != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.shortSquares.black)];
			if (state.castlingRooks.longSquares.black != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.longSquares.black)];
		}
		else
		{
			if (state.castlingRooks.shortSquares.white != Square::None)
				fen << 'K';
			if (state.castlingRooks.longSquares.white  != Square::None)
				fen << 'Q';
			if (state.castlingRooks.shortSquares.black != Square::None)
				fen << 'k';
			if (state.castlingRooks.longSquares.black  != Square::None)
				fen << 'q';
		}

		if (state.enPassant != Square::None)
			fen << ' ' << squareToString(state.enPassant);
		else fen << " -";

		fen << ' ' << state.halfmove;
		fen << ' ' << m_fullmove;

		return fen.str();
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::setPiece(Piece piece, Square square, eval::NnueState *nnueState) -> void
	{
		auto &state = currState();

		state.boards.setPiece(square, piece);

		if (basePiece(piece) == BasePiece::King)
			state.king(pieceColor(piece)) = square;

		if constexpr (UpdateNnue)
			nnueState->activateFeature(piece, square);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, square);
			state.key ^= hash;
		}
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::removePiece(Piece piece, Square square, eval::NnueState *nnueState) -> void
	{
		auto &state = currState();

		state.boards.removePiece(square, piece);

		if constexpr (UpdateNnue)
			nnueState->deactivateFeature(piece, square);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, square);
			state.key ^= hash;
		}
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::movePieceNoCap(Piece piece, Square src, Square dst, eval::NnueState *nnueState) -> void
	{
		auto &state = currState();

		state.boards.movePiece(src, dst, piece);

		if (basePiece(piece) == BasePiece::King)
			state.king(pieceColor(piece)) = dst;

		if constexpr (UpdateNnue)
			nnueState->moveFeature(piece, src, dst);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, src) ^ hash::pieceSquare(piece, dst);
			state.key ^= hash;
		}
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::movePiece(Piece piece, Square src, Square dst, eval::NnueState *nnueState) -> Piece
	{
		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			state.boards.removePiece(dst, captured);

			if constexpr (UpdateNnue)
				nnueState->deactivateFeature(captured, dst);

			if constexpr (UpdateKey)
			{
				const auto hash = hash::pieceSquare(captured, dst);
				state.key ^= hash;
			}
		}

		state.boards.movePiece(src, dst, piece);

		if (basePiece(piece) == BasePiece::King)
			state.king(pieceColor(piece)) = dst;

		if constexpr (UpdateNnue)
			nnueState->moveFeature(piece, src, dst);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(piece, src) ^ hash::pieceSquare(piece, dst);
			state.key ^= hash;
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::promotePawn(Piece pawn, Square src, Square dst,
		BasePiece target, eval::NnueState *nnueState) -> Piece
	{
		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			state.boards.removePiece(dst, captured);

			if constexpr (UpdateNnue)
				nnueState->deactivateFeature(captured, dst);

			if constexpr (UpdateKey)
				state.key ^= hash::pieceSquare(captured, dst);
		}

		state.boards.moveAndChangePiece(src, dst, pawn, target);

		if constexpr(UpdateNnue || UpdateKey)
		{
			const auto coloredTarget = copyPieceColor(pawn, target);

			if constexpr (UpdateNnue)
			{
				nnueState->deactivateFeature(pawn, src);
				nnueState->activateFeature(coloredTarget, dst);
			}

			if constexpr (UpdateKey)
			{
				state.key ^= hash::pieceSquare(pawn, src) ^ hash::pieceSquare(coloredTarget, dst);
			}
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::castle(Piece king, Square kingSrc, Square rookSrc, eval::NnueState *nnueState) -> void
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

		const auto rook = copyPieceColor(king, BasePiece::Rook);

		if (g_opts.chess960)
		{
			removePiece<UpdateKey, UpdateNnue>(rook, rookSrc, nnueState);

			if (kingSrc != kingDst)
				movePieceNoCap<UpdateKey, UpdateNnue>(king, kingSrc, kingDst, nnueState);

			setPiece<UpdateKey, UpdateNnue>(rook, rookDst, nnueState);
		}
		else
		{
			movePieceNoCap<UpdateKey, UpdateNnue>(king, kingSrc, kingDst, nnueState);
			movePieceNoCap<UpdateKey, UpdateNnue>(rook, rookSrc, rookDst, nnueState);
		}
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::enPassant(Piece pawn, Square src, Square dst, eval::NnueState *nnueState) -> Piece
	{
		auto &state = currState();

		const auto color = pieceColor(pawn);

		state.boards.movePiece(src, dst, pawn);

		if constexpr (UpdateNnue)
			nnueState->moveFeature(pawn, src, dst);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(pawn, src) ^ hash::pieceSquare(pawn, dst);
			state.key ^= hash;
		}

		auto rank = squareRank(dst);
		const auto file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto captureSquare = toSquare(rank, file);
		const auto enemyPawn = flipPieceColor(pawn);

		state.boards.removePiece(captureSquare, enemyPawn);

		if constexpr (UpdateNnue)
			nnueState->deactivateFeature(enemyPawn, captureSquare);

		if constexpr (UpdateKey)
		{
			const auto hash = hash::pieceSquare(enemyPawn, captureSquare);
			state.key ^= hash;
		}

		return enemyPawn;
	}

	template <bool EnPassantFromMoves>
	auto Position::regen() -> void
	{
		auto &state = currState();

		state.key = 0;

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				const auto square = toSquare(rank, file);
				if (const auto piece = state.boards.pieceAt(square); piece != Piece::None)
				{
					if (basePiece(piece) == BasePiece::King)
						state.king(pieceColor(piece)) = square;

					const auto hash = hash::pieceSquare(piece, toSquare(rank, file));
					state.key ^= hash;
				}
			}
		}

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

		state.key ^= hash::castling(state.castlingRooks);
		state.key ^= hash::enPassant(state.enPassant);

		state.checkers = calcCheckers();
		state.threats = calcThreats();
	}

#ifndef NDEBUG
	auto Position::printHistory(Move last) -> void
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

	template <bool HasHistory>
	auto Position::verify() -> bool
	{
		Position regened{*this};
		regened.regen<HasHistory>();

		std::ostringstream out{};
		out << std::hex << std::uppercase;

		bool failed = false;

#define SP_CHECK(A, B, Str) \
		if ((A) != (B)) \
		{ \
			out << "info string " Str " do not match"; \
			out << "\ninfo string current: " << std::setw(16) << std::setfill('0') << (A); \
			out << "\ninfo string regened: " << std::setw(16) << std::setfill('0') << (B); \
			out << '\n'; \
			failed = true; \
		}

		out << std::dec;
		SP_CHECK(static_cast<u64>(currState().enPassant), static_cast<u64>(regened.currState().enPassant), "en passant squares")
		out << std::hex;

		SP_CHECK(currState().key, regened.currState().key, "keys")

		out << std::dec;

#undef SP_CHECK_PIECES
#undef SP_CHECK_PIECE
#undef SP_CHECK

		if (failed)
			std::cout << out.view() << std::flush;

		return !failed;
	}
#endif

	auto Position::moveFromUci(const std::string &move) const -> Move
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
					if (state.boards.pieceAt(dst) == copyPieceColor(srcPiece, BasePiece::Rook))
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
		position.resetToStarting();
		return position;
	}

	auto Position::fromFen(const std::string &fen) -> std::optional<Position>
	{
		Position position{};

		if (position.resetFromFen(fen))
			return position;

		return {};
	}

	auto Position::fromFrcIndex(u32 n) -> std::optional<Position>
	{
		assert(g_opts.chess960);

		if (n >= 960)
		{
			std::cerr << "invalid frc position index " << n << std::endl;
			return {};
		}

		Position position{};
		position.resetFromFrcIndex(n);

		return position;
	}

	auto Position::fromDfrcIndex(u32 n) -> std::optional<Position>
	{
		assert(g_opts.chess960);

		if (n >= 960 * 960)
		{
			std::cerr << "invalid dfrc position index " << n << std::endl;
			return {};
		}

		Position position{};
		position.resetFromDfrcIndex(n);

		return position;
	}

	auto squareFromString(const std::string &str) -> Square
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
