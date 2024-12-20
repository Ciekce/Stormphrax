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

#include "../util/parse.h"
#include "../util/split.h"
#include "../attacks/attacks.h"
#include "../movegen.h"
#include "../opts.h"
#include "../rays.h"
#include "../cuckoo.h"

namespace stormphrax
{
	namespace
	{
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

			std::array<PieceType, 8> dst{};
			// no need to fill with empty pieces, because pawns are impossible

			const auto placeInNthFree = [&dst](u32 n, PieceType piece)
			{
				for (i32 free = 0, i = 0; i < 8; ++i)
				{
					if (dst[i] == PieceType::Pawn
						&& free++ == n)
					{
						dst[i] = piece;
						break;
					}
				}
			};

			const auto placeInFirstFree = [&dst](PieceType piece)
			{
				for (i32 i = 0; i < 8; ++i)
				{
					if (dst[i] == PieceType::Pawn)
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

			dst[b1 * 2 + 1] = PieceType::Bishop;
			dst[b2 * 2    ] = PieceType::Bishop;

			placeInNthFree(q, PieceType::Queen);

			const auto [knight1, knight2] = N5n[n4];

			placeInNthFree(knight1, PieceType::Knight);
			placeInNthFree(knight2, PieceType::Knight);

			placeInFirstFree(PieceType::Rook);
			placeInFirstFree(PieceType::King);
			placeInFirstFree(PieceType::Rook);

			return dst;
		}
	}

	template auto Position::applyMoveUnchecked<false, false>(Move, eval::NnueState *) -> void;
	template auto Position::applyMoveUnchecked<true, false>(Move, eval::NnueState *) -> void;
	template auto Position::applyMoveUnchecked<false, true>(Move, eval::NnueState *) -> void;
	template auto Position::applyMoveUnchecked<true, true>(Move, eval::NnueState *) -> void;

	template auto Position::popMove<false>(eval::NnueState *) -> void;
	template auto Position::popMove<true>(eval::NnueState *) -> void;

	template auto Position::setPiece<false>(Piece, Square) -> void;
	template auto Position::setPiece<true>(Piece, Square) -> void;

	template auto Position::removePiece<false>(Piece, Square) -> void;
	template auto Position::removePiece<true>(Piece, Square) -> void;

	template auto Position::movePieceNoCap<false>(Piece, Square, Square) -> void;
	template auto Position::movePieceNoCap<true>(Piece, Square, Square) -> void;

	template auto Position::movePiece<false, false>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::movePiece<true, false>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::movePiece<false, true>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::movePiece<true, true>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;

	template auto Position::promotePawn<false, false>(Piece, Square, Square, PieceType, eval::NnueUpdates &) -> Piece;
	template auto Position::promotePawn<true, false>(Piece, Square, Square, PieceType, eval::NnueUpdates &) -> Piece;
	template auto Position::promotePawn<false, true>(Piece, Square, Square, PieceType, eval::NnueUpdates &) -> Piece;
	template auto Position::promotePawn<true, true>(Piece, Square, Square, PieceType, eval::NnueUpdates &) -> Piece;

	template auto Position::castle<false, false>(Piece, Square, Square, eval::NnueUpdates &) -> void;
	template auto Position::castle<true, false>(Piece, Square, Square, eval::NnueUpdates &) -> void;
	template auto Position::castle<false, true>(Piece, Square, Square, eval::NnueUpdates &) -> void;
	template auto Position::castle<true, true>(Piece, Square, Square, eval::NnueUpdates &) -> void;

	template auto Position::enPassant<false, false>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::enPassant<true, false>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::enPassant<false, true>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;
	template auto Position::enPassant<true, true>(Piece, Square, Square, eval::NnueUpdates &) -> Piece;

	Position::Position()
	{
		m_states.reserve(256);
		m_keys.reserve(512);

		m_states.push_back({});
	}

	auto Position::resetToStarting() -> void
	{
		m_states.resize(1);
		m_keys.clear();

		auto &state = currState();
		state = BoardState{};

		auto &bbs = state.boards.bbs();

		bbs.forPiece(PieceType::  Pawn) = U64(0x00FF00000000FF00);
		bbs.forPiece(PieceType::Knight) = U64(0x4200000000000042);
		bbs.forPiece(PieceType::Bishop) = U64(0x2400000000000024);
		bbs.forPiece(PieceType::  Rook) = U64(0x8100000000000081);
		bbs.forPiece(PieceType:: Queen) = U64(0x0800000000000008);
		bbs.forPiece(PieceType::  King) = U64(0x1000000000000010);

		bbs.forColor(Color::Black) = U64(0xFFFF000000000000);
		bbs.forColor(Color::White) = U64(0x000000000000FFFF);

		state.castlingRooks.black().kingside  = Square::H8;
		state.castlingRooks.black().queenside = Square::A8;
		state.castlingRooks.white().kingside  = Square::H1;
		state.castlingRooks.white().queenside = Square::A1;

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
		auto &newBbs = newState.boards.bbs();

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

		if (const auto blackKingCount = newBbs.forPiece(Piece::BlackKing).popcount();
			blackKingCount != 1)
		{
			std::cerr << "black must have exactly 1 king, " << blackKingCount << " in fen " << fen << std::endl;
			return false;
		}

		if (const auto whiteKingCount = newBbs.forPiece(Piece::WhiteKing).popcount();
			whiteKingCount != 1)
		{
			std::cerr << "white must have exactly 1 king, " << whiteKingCount << " in fen " << fen << std::endl;
			return false;
		}

		if (newBbs.occupancy().popcount() > 32)
		{
			std::cerr << "too many pieces in fen " << fen << std::endl;
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
				newBbs.forPiece(PieceType::King, oppColor(stm)).lowestSquare(),
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
						if (piece != Piece::None && pieceType(piece) == PieceType::King)
							newState.kings.color(pieceColor(piece)) = square;
					}
				}

				for (const auto flag : castlingFlags)
				{
					if (flag >= 'a' && flag <= 'h')
					{
						const auto file = static_cast<i32>(flag - 'a');
						const auto kingFile = squareFile(newState.kings.black());

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return false;
						}

						if (file < kingFile)
							newState.castlingRooks.black().queenside = toSquare(7, file);
						else newState.castlingRooks.black().kingside = toSquare(7, file);
					}
					else if (flag >= 'A' && flag <= 'H')
					{
						const auto file = static_cast<i32>(flag - 'A');
						const auto kingFile = squareFile(newState.kings.white());

						if (file == kingFile)
						{
							std::cerr << "invalid castling availability in fen " << fen << std::endl;
							return false;
						}

						if (file < kingFile)
							newState.castlingRooks.white().queenside = toSquare(0, file);
						else newState.castlingRooks.white().kingside = toSquare(0, file);
					}
					else if (flag == 'k')
					{
						for (i32 file = squareFile(newState.kings.black()) + 1; file < 8; ++file)
						{
							const auto square = toSquare(7, file);
							if (newState.boards.pieceAt(square) == Piece::BlackRook)
							{
								newState.castlingRooks.black().kingside = square;
								break;
							}
						}
					}
					else if (flag == 'K')
					{
						for (i32 file = squareFile(newState.kings.white()) + 1; file < 8; ++file)
						{
							const auto square = toSquare(0, file);
							if (newState.boards.pieceAt(square) == Piece::WhiteRook)
							{
								newState.castlingRooks.white().kingside = square;
								break;
							}
						}
					}
					else if (flag == 'q')
					{
						for (i32 file = squareFile(newState.kings.black()) - 1; file >= 0; --file)
						{
							const auto square = toSquare(7, file);
							if (newState.boards.pieceAt(square) == Piece::BlackRook)
							{
								newState.castlingRooks.black().queenside = square;
								break;
							}
						}
					}
					else if (flag == 'Q')
					{
						for (i32 file = squareFile(newState.kings.white()) - 1; file >= 0; --file)
						{
							const auto square = toSquare(0, file);
							if (newState.boards.pieceAt(square) == Piece::WhiteRook)
							{
								newState.castlingRooks.white().queenside = square;
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
				for (const auto flag : castlingFlags)
				{
					switch (flag)
					{
					case 'k': newState.castlingRooks.black().kingside  = Square::H8; break;
					case 'q': newState.castlingRooks.black().queenside = Square::A8; break;
					case 'K': newState.castlingRooks.white().kingside  = Square::H1; break;
					case 'Q': newState.castlingRooks.white().queenside = Square::A1; break;
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
		m_keys.clear();

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
		m_keys.clear();

		auto &state = currState();
		state = BoardState{};

		auto &bbs = state.boards.bbs();

		bbs.forPiece(PieceType::Pawn) = U64(0x00FF00000000FF00);

		bbs.forColor(Color::Black) = U64(0x00FF000000000000);
		bbs.forColor(Color::White) = U64(0x000000000000FF00);

		const auto backrank = scharnaglToBackrank(n);

		bool firstRook = true;

		for (i32 i = 0; i < 8; ++i)
		{
			const auto blackSquare = toSquare(7, i);
			const auto whiteSquare = toSquare(0, i);

			state.boards.setPiece(blackSquare, colorPiece(backrank[i], Color::Black));
			state.boards.setPiece(whiteSquare, colorPiece(backrank[i], Color::White));

			if (backrank[i] == PieceType::Rook)
			{
				if (firstRook)
				{
					state.castlingRooks.black().queenside = blackSquare;
					state.castlingRooks.white().queenside = whiteSquare;
				}
				else
				{
					state.castlingRooks.black().kingside  = blackSquare;
					state.castlingRooks.white().kingside  = whiteSquare;
				}

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
		m_keys.clear();

		auto &state = currState();
		state = BoardState{};

		auto &bbs = state.boards.bbs();

		bbs.forPiece(PieceType::Pawn) = U64(0x00FF00000000FF00);

		bbs.forColor(Color::Black) = U64(0x00FF000000000000);
		bbs.forColor(Color::White) = U64(0x000000000000FF00);

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

			if (blackBackrank[i] == PieceType::Rook)
			{
				if (firstBlackRook)
					state.castlingRooks.black().queenside = blackSquare;
				else state.castlingRooks.black().kingside = blackSquare;

				firstBlackRook = false;
			}

			if (whiteBackrank[i] == PieceType::Rook)
			{
				if (firstWhiteRook)
					state.castlingRooks.white().queenside = whiteSquare;
				else state.castlingRooks.white().kingside = whiteSquare;

				firstWhiteRook = false;
			}
		}

		m_blackToMove = false;
		m_fullmove = 1;

		regen();

		return true;
	}

	auto Position::copyStateFrom(const Position &other) -> void
	{
		m_states.clear();
		m_keys.clear();

		m_states.push_back(other.currState());

		m_blackToMove = other.m_blackToMove;
		m_fullmove = other.m_fullmove;
	}

	template <bool UpdateNnue, bool StateHistory>
	auto Position::applyMoveUnchecked(Move move, eval::NnueState *nnueState) -> void
	{
		if constexpr (UpdateNnue)
			assert(nnueState != nullptr);

		auto &prevState = currState();

		if constexpr (StateHistory)
		{
			assert(m_states.size() < m_states.capacity());
			m_states.push_back(prevState);
		}

		m_keys.push_back(prevState.keys.all);

		auto &state = currState();

		m_blackToMove = !m_blackToMove;

		state.keys.flipStm();

		if (state.enPassant != Square::None)
		{
			state.keys.flipEp(state.enPassant);
			state.enPassant = Square::None;
		}

		const auto stm = opponent();
		const auto nstm = oppColor(stm);

		if (stm == Color::Black)
			++m_fullmove;

		if (!move)
		{
			state.pinned = calcPinned();
			state.threats = calcThreats();

			return;
		}

		const auto moveType = move.type();

		const auto moveSrc = move.src();
		const auto moveDst = move.dst();

		auto newCastlingRooks = state.castlingRooks;

		const auto moving = state.boards.pieceAt(moveSrc);
		const auto movingType = pieceType(moving);

		eval::NnueUpdates updates{};
		auto captured = Piece::None;

		switch (moveType)
		{
		case MoveType::Standard:
			captured = movePiece<true, UpdateNnue>(moving, moveSrc, moveDst, updates);
			break;
		case MoveType::Promotion:
			captured = promotePawn<true, UpdateNnue>(moving, moveSrc, moveDst, move.promo(), updates);
			break;
		case MoveType::Castling:
			castle<true, UpdateNnue>(moving, moveSrc, moveDst, updates);
			break;
		case MoveType::EnPassant:
			captured = enPassant<true, UpdateNnue>(moving, moveSrc, moveDst, updates);
			break;
		}

		assert(pieceTypeOrNone(captured) != PieceType::King);

		if constexpr (UpdateNnue)
			nnueState->pushUpdates<!StateHistory>(updates, state.boards.bbs(), state.kings);

		if (movingType == PieceType::Rook)
			newCastlingRooks.color(stm).unset(moveSrc);
		else if (movingType == PieceType::King)
			newCastlingRooks.color(stm).clear();
		else if (moving == Piece::BlackPawn && move.srcRank() == 6 && move.dstRank() == 4)
		{
			state.enPassant = toSquare(5, move.srcFile());
			state.keys.flipEp(state.enPassant);
		}
		else if (moving == Piece::WhitePawn && move.srcRank() == 1 && move.dstRank() == 3)
		{
			state.enPassant = toSquare(2, move.srcFile());
			state.keys.flipEp(state.enPassant);
		}

		if (captured == Piece::None
			&& pieceType(moving) != PieceType::Pawn)
			++state.halfmove;
		else state.halfmove = 0;

		if (captured != Piece::None && pieceType(captured) == PieceType::Rook)
			newCastlingRooks.color(nstm).unset(moveDst);

		if (newCastlingRooks != state.castlingRooks)
		{
			state.keys.switchCastling(state.castlingRooks, newCastlingRooks);
			state.castlingRooks = newCastlingRooks;
		}

		state.checkers = calcCheckers();
		state.pinned = calcPinned();
		state.threats = calcThreats();

		filterEp(state, nstm);
	}

	template <bool UpdateNnue>
	auto Position::popMove(eval::NnueState *nnueState) -> void
	{
		assert(m_states.size() > 1 && "popMove() with no previous move?");

		if constexpr (UpdateNnue)
		{
			assert(nnueState != nullptr);
			nnueState->pop();
		}

		m_states.pop_back();
		m_keys.pop_back();

		m_blackToMove = !m_blackToMove;

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
		assert(move != NullMove);

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
					|| dstPiece != colorPiece(PieceType::Rook, us)))
				// or trying to capture a king
				|| pieceType(dstPiece) == PieceType::King))
			return false;

		const auto srcPieceType = pieceType(srcPiece);
		const auto them = oppColor(us);
		const auto occ = state.boards.bbs().occupancy();

		if (type == MoveType::Castling)
		{
			if (srcPieceType != PieceType::King || isCheck())
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
				if (dst != state.castlingRooks.color(us).kingside)
					return false;

				kingDst = toSquare(rank, 6);
				rookDst = toSquare(rank, 5);
			}
			else
			{
				// no castling rights
				if (dst != state.castlingRooks.color(us).queenside)
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
				if (dst == state.castlingRooks.black().kingside)
					return (occ & U64(0x6000000000000000)).empty()
						&& !isAttacked(Square::F8, Color::White);
				else if (dst == state.castlingRooks.black().queenside)
					return (occ & U64(0x0E00000000000000)).empty()
						&& !isAttacked(Square::D8, Color::White);
				else if (dst == state.castlingRooks.white().kingside)
					return (occ & U64(0x0000000000000060)).empty()
						&& !isAttacked(Square::F1, Color::Black);
				else return (occ & U64(0x000000000000000E)).empty()
						&& !isAttacked(Square::D1, Color::Black);
			}
		}

		if (srcPieceType == PieceType::Pawn)
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
				if (!(attacks::getPawnAttacks(src, us) & state.boards.bbs().forColor(them))[dst])
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

			switch (srcPieceType)
			{
			case PieceType::Knight: attacks = attacks::getKnightAttacks(src); break;
			case PieceType::Bishop: attacks = attacks::getBishopAttacks(src, occ); break;
			case PieceType::  Rook: attacks = attacks::getRookAttacks(src, occ); break;
			case PieceType:: Queen: attacks = attacks::getQueenAttacks(src, occ); break;
			case PieceType::  King: attacks = attacks::getKingAttacks(src); break;
			default: __builtin_unreachable();
			}

			if (!attacks[dst])
				return false;
		}

		return true;
	}

	// This does *not* check for pseudolegality, moves are assumed to be pseudolegal
	auto Position::isLegal(Move move) const -> bool
	{
		assert(move != NullMove);

		const auto us = toMove();
		const auto them = oppColor(us);

		const auto &state = currState();
		const auto &bbs = state.boards.bbs();

		const auto src = move.src();
		const auto dst = move.dst();

		const auto king = state.kings.color(us);

		if (move.type() == MoveType::Castling)
		{
			const auto kingDst = toSquare(move.srcRank(), move.srcFile() < move.dstFile() ? 6 : 2);
			return !state.threats[kingDst] && !(g_opts.chess960 && state.pinned[dst]);
		}
		else if (move.type() == MoveType::EnPassant)
		{
			auto rank = squareRank(dst);
			const auto file = squareFile(dst);

			rank = rank == 2 ? 3 : 4;

			const auto captureSquare = toSquare(rank, file);

			const auto postEpOcc = bbs.occupancy()
				^ Bitboard::fromSquare(src)
				^ Bitboard::fromSquare(dst)
				^ Bitboard::fromSquare(captureSquare);

			const auto theirQueens = bbs.queens(them);

			return (attacks::getBishopAttacks(king, postEpOcc) & (theirQueens | bbs.bishops(them))).empty()
				&& (attacks::getRookAttacks  (king, postEpOcc) & (theirQueens | bbs.  rooks(them))).empty();
		}

		const auto moving = state.boards.pieceAt(src);

		if (pieceType(moving) == PieceType::King)
		{
			const auto kinglessOcc = bbs.occupancy() ^ bbs.kings(us);
			const auto theirQueens = bbs.queens(them);

			return !state.threats[move.dst()]
				&& (attacks::getBishopAttacks(dst, kinglessOcc) & (theirQueens | bbs.bishops(them))).empty()
				&& (attacks::getRookAttacks  (dst, kinglessOcc) & (theirQueens | bbs.  rooks(them))).empty();
		}

		// multiple checks can only be evaded with a king move
		if (state.checkers.multiple()
			|| state.pinned[src] && !rayIntersecting(src, dst)[king])
			return false;

		if (state.checkers.empty())
			return true;

		const auto checker = state.checkers.lowestSquare();
		return (rayBetween(king, checker) | Bitboard::fromSquare(checker))[dst];
	}

	// see comment in cuckoo.cpp
	auto Position::hasCycle(i32 ply) const -> bool
	{
		const auto &state = currState();

		const auto end = std::min<i32>(state.halfmove, static_cast<i32>(m_keys.size()));

		if (end < 3)
			return false;

		const auto S = [this](i32 d)
		{
			return m_keys[m_keys.size() - d];
		};

		const auto occ = state.boards.bbs().occupancy();
		const auto originalKey = state.keys.all;

		auto other = ~(originalKey ^ S(1));

		for (i32 d = 3; d <= end; d += 2)
		{
			const auto currKey = S(d);

			other ^= ~(currKey ^ S(d - 1));
			if (other != 0)
				continue;

			const auto diff = originalKey ^ currKey;

			u32 slot = cuckoo::h1(diff);

			if (diff != cuckoo::keys[slot])
				slot = cuckoo::h2(diff);

			if (diff != cuckoo::keys[slot])
				continue;

			const auto move = cuckoo::moves[slot];

			if ((occ & rayBetween(move.src(), move.dst())).empty())
			{
				// repetition is after root, done
				if (ply > d)
					return true;

				auto piece = state.boards.pieceAt(move.src());
				if (piece == Piece::None)
					piece = state.boards.pieceAt(move.dst());

				assert(piece != Piece::None);

				return pieceColor(piece) == toMove();
			}
		}

		return false;
	}

	auto Position::isDrawn(bool threefold) const -> bool
	{
		const auto halfmove = currState().halfmove;

		if (halfmove >= 100)
		{
			if (!isCheck())
				return true;

			//TODO there's a speedup possible here, but
			// it requires a lot of movegen refactoring
			ScoredMoveList moves{};
			generateAll(moves, *this);

			return std::ranges::any_of(moves, [this](const auto move)
			{
				return isLegal(move.move);
			});
		}

		const auto currKey = currState().keys.all;
		const auto limit = std::max(0, static_cast<i32>(m_keys.size()) - halfmove - 2);

		i32 repetitionsLeft = threefold ? 2 : 1;

		for (auto i = static_cast<i32>(m_keys.size()) - 4; i >= limit; i -= 2)
		{
			if (m_keys[i] == currKey
				&& --repetitionsLeft == 0)
				return true;
		}

		const auto &bbs = this->bbs();

		if (!bbs.pawns().empty() || !bbs.majors().empty())
			return false;

		// KK
		if (bbs.nonPk().empty())
			return true;

		// KNK or KBK
		if ((bbs.blackNonPk().empty() && bbs.whiteNonPk() == bbs.whiteMinors() && !bbs.whiteMinors().multiple())
			|| (bbs.whiteNonPk().empty() && bbs.blackNonPk() == bbs.blackMinors() && !bbs.blackMinors().multiple()))
			return true;

		// KBKB OCB
		if ((bbs.blackNonPk() == bbs.blackBishops() && bbs.whiteNonPk() == bbs.whiteBishops())
			&& !bbs.blackBishops().multiple() && !bbs.whiteBishops().multiple()
			&& (bbs.blackBishops() & boards::LightSquares).empty() != (bbs.whiteBishops() & boards::LightSquares).empty())
			return true;

		return false;
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

			if (state.castlingRooks.white().kingside != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.white().kingside)];
			if (state.castlingRooks.white().queenside != Square::None)
				fen << WhiteFiles[squareFile(state.castlingRooks.white().queenside)];
			if (state.castlingRooks.black().kingside != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.black().kingside)];
			if (state.castlingRooks.black().queenside != Square::None)
				fen << BlackFiles[squareFile(state.castlingRooks.black().queenside)];
		}
		else
		{
			if (state.castlingRooks.white().kingside  != Square::None)
				fen << 'K';
			if (state.castlingRooks.white().queenside != Square::None)
				fen << 'Q';
			if (state.castlingRooks.black().kingside  != Square::None)
				fen << 'k';
			if (state.castlingRooks.black().queenside != Square::None)
				fen << 'q';
		}

		if (state.enPassant != Square::None)
			fen << ' ' << squareToString(state.enPassant);
		else fen << " -";

		fen << ' ' << state.halfmove;
		fen << ' ' << m_fullmove;

		return fen.str();
	}

	template <bool UpdateKey>
	auto Position::setPiece(Piece piece, Square square) -> void
	{
		assert(piece != Piece::None);
		assert(square != Square::None);

		assert(pieceType(piece) != PieceType::King);

		auto &state = currState();

		state.boards.setPiece(square, piece);

		if constexpr (UpdateKey)
			state.keys.flipPiece(piece, square);
	}

	template <bool UpdateKey>
	auto Position::removePiece(Piece piece, Square square) -> void
	{
		assert(piece != Piece::None);
		assert(square != Square::None);

		assert(pieceType(piece) != PieceType::King);

		auto &state = currState();

		state.boards.removePiece(square, piece);

		if constexpr (UpdateKey)
			state.keys.flipPiece(piece, square);
	}

	template <bool UpdateKey>
	auto Position::movePieceNoCap(Piece piece, Square src, Square dst) -> void
	{
		assert(piece != Piece::None);

		assert(src != Square::None);
		assert(dst != Square::None);

		if (src == dst)
			return;

		auto &state = currState();

		state.boards.movePiece(src, dst, piece);

		if (pieceType(piece) == PieceType::King)
		{
			const auto color = pieceColor(piece);
			state.kings.color(color) = dst;
		}

		if constexpr (UpdateKey)
			state.keys.movePiece(piece, src, dst);
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::movePiece(Piece piece, Square src, Square dst, eval::NnueUpdates &nnueUpdates) -> Piece
	{
		assert(piece != Piece::None);

		assert(src != Square::None);
		assert(dst != Square::None);
		assert(src != dst);

		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			assert(pieceType(captured) != PieceType::King);

			state.boards.removePiece(dst, captured);

			// NNUE update done below

			if constexpr (UpdateKey)
				state.keys.flipPiece(captured, dst);
		}

		state.boards.movePiece(src, dst, piece);

		if (pieceType(piece) == PieceType::King)
		{
			const auto color = pieceColor(piece);

			if constexpr (UpdateNnue)
			{
				if (eval::InputFeatureSet::refreshRequired(color, state.kings.color(color), dst))
					nnueUpdates.setRefresh(color);
			}

			state.kings.color(color) = dst;
		}

		if constexpr (UpdateNnue)
		{
			nnueUpdates.pushSubAdd(piece, src, dst);

			if (captured != Piece::None)
				nnueUpdates.pushSub(captured, dst);
		}

		if constexpr (UpdateKey)
			state.keys.movePiece(piece, src, dst);

		return captured;
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::promotePawn(Piece pawn, Square src, Square dst,
		PieceType promo, eval::NnueUpdates &nnueUpdates) -> Piece
	{
		assert(pawn != Piece::None);
		assert(pieceType(pawn) == PieceType::Pawn);

		assert(src != Square::None);
		assert(dst != Square::None);
		assert(src != dst);

		assert(squareRank(dst) == relativeRank(pieceColor(pawn), 7));
		assert(squareRank(src) == relativeRank(pieceColor(pawn), 6));

		assert(promo != PieceType::None);

		auto &state = currState();

		const auto captured = state.boards.pieceAt(dst);

		if (captured != Piece::None)
		{
			assert(pieceType(captured) != PieceType::King);

			state.boards.removePiece(dst, captured);

			if constexpr (UpdateNnue)
				nnueUpdates.pushSub(captured, dst);

			if constexpr (UpdateKey)
				state.keys.flipPiece(captured, dst);
		}

		state.boards.moveAndChangePiece(src, dst, pawn, promo);

		if constexpr(UpdateNnue || UpdateKey)
		{
			const auto coloredPromo = copyPieceColor(pawn, promo);

			if constexpr (UpdateNnue)
			{
				nnueUpdates.pushSub(pawn, src);
				nnueUpdates.pushAdd(coloredPromo, dst);
			}

			if constexpr (UpdateKey)
			{
				state.keys.flipPiece(pawn, src);
				state.keys.flipPiece(coloredPromo, dst);
			}
		}

		return captured;
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::castle(Piece king, Square kingSrc, Square rookSrc, eval::NnueUpdates &nnueUpdates) -> void
	{
		assert(king != Piece::None);
		assert(pieceType(king) == PieceType::King);

		assert(kingSrc != Square::None);
		assert(rookSrc != Square::None);
		assert(kingSrc != rookSrc);

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

		const auto rook = copyPieceColor(king, PieceType::Rook);

		movePieceNoCap<UpdateKey>(king, kingSrc, kingDst);
		movePieceNoCap<UpdateKey>(rook, rookSrc, rookDst);

		if constexpr (UpdateNnue)
		{
			const auto color = pieceColor(king);

			if (eval::InputFeatureSet::refreshRequired(color, kingSrc, kingDst))
				nnueUpdates.setRefresh(color);

			nnueUpdates.pushSubAdd(king, kingSrc, kingDst);
			nnueUpdates.pushSubAdd(rook, rookSrc, rookDst);
		}
	}

	template <bool UpdateKey, bool UpdateNnue>
	auto Position::enPassant(Piece pawn, Square src, Square dst, eval::NnueUpdates &nnueUpdates) -> Piece
	{
		assert(pawn != Piece::None);
		assert(pieceType(pawn) == PieceType::Pawn);

		assert(src != Square::None);
		assert(dst != Square::None);
		assert(src != dst);

		auto &state = currState();

		state.boards.movePiece(src, dst, pawn);

		if constexpr (UpdateNnue)
			nnueUpdates.pushSubAdd(pawn, src, dst);

		if constexpr (UpdateKey)
			state.keys.movePiece(pawn, src, dst);

		auto rank = squareRank(dst);
		const auto file = squareFile(dst);

		rank = rank == 2 ? 3 : 4;

		const auto captureSquare = toSquare(rank, file);
		const auto enemyPawn = flipPieceColor(pawn);

		state.boards.removePiece(captureSquare, enemyPawn);

		if constexpr (UpdateNnue)
			nnueUpdates.pushSub(enemyPawn, captureSquare);

		if constexpr (UpdateKey)
			state.keys.flipPiece(enemyPawn, captureSquare);

		return enemyPawn;
	}

	auto Position::regen() -> void
	{
		auto &state = currState();

		state.boards.regenFromBbs();

		state.keys.clear();

		for (u32 rank = 0; rank < 8; ++rank)
		{
			for (u32 file = 0; file < 8; ++file)
			{
				const auto square = toSquare(rank, file);
				if (const auto piece = state.boards.pieceAt(square); piece != Piece::None)
				{
					if (pieceType(piece) == PieceType::King)
						state.kings.color(pieceColor(piece)) = square;

					state.keys.flipPiece(piece, toSquare(rank, file));
				}
			}
		}

		state.keys.flipCastling(state.castlingRooks);
		state.keys.flipEp(state.enPassant);

		if (toMove() == Color::Black)
			state.keys.flipStm();

		state.checkers = calcCheckers();
		state.pinned = calcPinned();
		state.threats = calcThreats();

		filterEp(state, toMove());
	}

	void Position::filterEp(BoardState &state, Color capturing)
	{
		if (state.enPassant == Square::None)
			return;

		const auto unset = [&]
		{
			state.keys.flipEp(state.enPassant);
			state.enPassant = Square::None;
		};

		const auto &bbs = state.boards.bbs();

		const auto moved = oppColor(capturing);

		const auto king = state.kings.color(capturing);

		const auto candidates = bbs.pawns(capturing) & attacks::getPawnAttacks(state.enPassant, moved);
		const auto vertPinned = state.pinned & boards::Files[squareFile(king)];

		// vertically pinned pawns cannot capture at all
		const auto pawns = candidates & ~vertPinned;

		if (!pawns)
		{
			unset();
			return;
		}

		// if there are multiple pawns available, they can't both be
		// pinned and neither capture can result in a discovered check
		if (candidates.multiple())
			return;

		const auto diagPinned = pawns & state.pinned;

		// if the capturing pawn is pinned, it has to be pinned
		// along the same diagonal that the capture would occur
		if (diagPinned)
		{
			const auto pinnedPawn = diagPinned.lowestSquare();
			const auto pinRay = attacks::getBishopAttacks(king, bbs.occupancy(moved))
				& rayIntersecting(king, pinnedPawn);

			if (!pinRay[state.enPassant])
			{
				unset();
				return;
			}
		}

		// also handle the annoying case where capturing en passant would cause discovered check
		const auto movedPawn = toSquare(squareRank(state.enPassant)
			+ (moved == Color::White ? 1 : -1), squareFile(state.enPassant));
		const auto capturingPawn = candidates.lowestSquare();

		const auto rank = rayIntersecting(movedPawn, capturingPawn);
		const auto oppRookCandidates = rank & (bbs.rooks(moved) | bbs.queens(moved));

		// not possible :3
		if (!rank[king] || !oppRookCandidates)
			return;

		const auto pawnlessOcc = bbs.occupancy() ^ squareBit(movedPawn) ^ squareBit(capturingPawn);
		const auto attacks = attacks::getRookAttacks(king, pawnlessOcc);

		if (attacks & oppRookCandidates)
			unset();
	}

	auto Position::moveFromUci(const std::string &move) const -> Move
	{
		if (move.length() < 4 || move.length() > 5)
			return NullMove;

		const auto src = squareFromString(move.substr(0, 2));
		const auto dst = squareFromString(move.substr(2, 2));

		if (move.length() == 5)
			return Move::promotion(src, dst, pieceTypeFromChar(move[4]));
		else
		{
			const auto &state = currState();

			const auto srcPiece = state.boards.pieceAt(src);

			if (srcPiece == Piece::BlackKing || srcPiece == Piece::WhiteKing)
			{
				if (g_opts.chess960)
				{
					if (state.boards.pieceAt(dst) == copyPieceColor(srcPiece, PieceType::Rook))
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

	auto Position::starting() -> Position
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

		const auto file = str[0];
		const auto rank = str[1];

		if (file < 'a' || file > 'h'
			|| rank < '1' || rank > '8')
			return Square::None;

		return toSquare(static_cast<u32>(rank - '1'), static_cast<u32>(file - 'a'));
	}
}
