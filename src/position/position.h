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

#pragma once

#include "../types.h"

#include <string>
#include <vector>
#include <stack>
#include <optional>
#include <utility>

#include "boards.h"
#include "../move.h"
#include "../attacks/attacks.h"
#include "../ttable.h"
#include "../eval/nnue.h"
#include "../rays.h"
#include "../keys.h"

namespace stormphrax
{
	struct Keys
	{
		u64 all;
		u64 pawns;
		u64 blackNonPawns;
		u64 whiteNonPawns;
		u64 majors;

		inline auto clear()
		{
			all = 0;
			pawns = 0;
			blackNonPawns = 0;
			whiteNonPawns = 0;
			majors = 0;
		}

		inline auto flipStm()
		{
			const auto key = keys::color();

			all ^= key;
			pawns ^= key;
			blackNonPawns ^= key;
			whiteNonPawns ^= key;
			majors ^= key;
		}

		inline auto flipPiece(Piece piece, Square square)
		{
			const auto key = keys::pieceSquare(piece, square);

			all ^= key;

			if (pieceType(piece) == PieceType::Pawn)
				pawns ^= key;
			else if (pieceColor(piece) == Color::Black)
				blackNonPawns ^= key;
			else whiteNonPawns ^= key;

			if (isMajor(piece))
				majors ^= key;
		}

		inline auto movePiece(Piece piece, Square src, Square dst)
		{
			const auto key = keys::pieceSquare(piece, src) ^ keys::pieceSquare(piece, dst);

			all ^= key;

			if (pieceType(piece) == PieceType::Pawn)
				pawns ^= key;
			else if (pieceColor(piece) == Color::Black)
				blackNonPawns ^= key;
			else whiteNonPawns ^= key;

			if (isMajor(piece))
				majors ^= key;
		}

		inline auto flipEp(Square epSq)
		{
			const auto key = keys::enPassant(epSq);

			all ^= key;
			pawns ^= key;
		}

		inline auto flipCastling(const CastlingRooks &rooks)
		{
			const auto key = keys::castling(rooks);

			all ^= key;
			blackNonPawns ^= key;
			whiteNonPawns ^= key;
			majors ^= key;
		}

		inline auto switchCastling(const CastlingRooks &before, const CastlingRooks &after)
		{
			const auto key = keys::castling(before) ^ keys::castling(after);

			all ^= key;
			blackNonPawns ^= key;
			whiteNonPawns ^= key;
			majors ^= key;
		}

		[[nodiscard]] inline auto operator==(const Keys &other) const -> bool = default;
	};

	struct BoardState
	{
		PositionBoards boards{};

		Keys keys{};

		Bitboard checkers{};
		Bitboard pinned{};
		Bitboard threats{};

		CastlingRooks castlingRooks{};

		u16 halfmove{};

		Square enPassant{Square::None};

		KingPair kings{};
	};

	static_assert(sizeof(BoardState) == 208);

	[[nodiscard]] inline auto squareToString(Square square)
	{
		constexpr auto Files = std::array{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
		constexpr auto Ranks = std::array{'1', '2', '3', '4', '5', '6', '7', '8'};

		const auto s = static_cast<u32>(square);
		return std::string{Files[s % 8], Ranks[s / 8]};
	}

	class Position;

	template <bool UpdateNnue>
	class HistoryGuard
	{
	public:
		explicit HistoryGuard(Position &pos, eval::NnueState *nnueState)
			: m_pos{pos},
			  m_nnueState{nnueState} {}
		inline ~HistoryGuard();

	private:
		Position &m_pos;
		eval::NnueState *m_nnueState;
	};

	class Position
	{
	public:
		Position();
		~Position() = default;

		Position(const Position &) = default;
		Position(Position &&) = default;

		auto resetToStarting() -> void;
		auto resetFromFen(const std::string &fen) -> bool;
		auto resetFromFrcIndex(u32 n) -> bool;
		auto resetFromDfrcIndex(u32 n) -> bool;

		auto copyStateFrom(const Position &other) -> void;

		// Moves are assumed to be legal
		template <bool UpdateNnue = true, bool StateHistory = true>
		auto applyMoveUnchecked(Move move, eval::NnueState *nnueState) -> void;

		// Moves are assumed to be legal
		template <bool UpdateNnue = true>
		[[nodiscard]] inline auto applyMove(Move move, eval::NnueState *nnueState)
		{
			if constexpr (UpdateNnue)
				assert(nnueState != nullptr);

			applyMoveUnchecked<UpdateNnue>(move, nnueState);

			return HistoryGuard<UpdateNnue>{*this, UpdateNnue ? nnueState : nullptr};
		}

		[[nodiscard]] inline auto applyNullMove()
		{
			return applyMove<false>(NullMove, nullptr);
		}

		template <bool UpdateNnue = true>
		auto popMove(eval::NnueState *nnueState) -> void;

		auto clearStateHistory() -> void;

		[[nodiscard]] auto isPseudolegal(Move move) const -> bool;
		[[nodiscard]] auto isLegal(Move move) const -> bool;

	private:
		[[nodiscard]] inline auto currState() -> auto & { return m_states.back(); }
		[[nodiscard]] inline auto currState() const -> const auto & { return m_states.back(); }

	public:
		[[nodiscard]] inline auto boards() const -> const auto & { return currState().boards; }
		[[nodiscard]] inline auto bbs() const -> const auto & { return currState().boards.bbs(); }

		[[nodiscard]] inline auto toMove() const
		{
			return m_blackToMove ? Color::Black : Color::White;
		}

		[[nodiscard]] inline auto opponent() const
		{
			return m_blackToMove ? Color::White : Color::Black;
		}

		[[nodiscard]] inline auto castlingRooks() const -> const auto & { return currState().castlingRooks; }

		[[nodiscard]] inline auto enPassant() const { return currState().enPassant; }

		[[nodiscard]] inline auto halfmove() const { return currState().halfmove; }
		[[nodiscard]] inline auto fullmove() const { return m_fullmove; }

		[[nodiscard]] inline auto key() const { return currState().keys.all; }
		[[nodiscard]] inline auto pawnKey() const { return currState().keys.pawns; }
		[[nodiscard]] inline auto blackNonPawnKey() const { return currState().keys.blackNonPawns; }
		[[nodiscard]] inline auto whiteNonPawnKey() const { return currState().keys.whiteNonPawns; }
		[[nodiscard]] inline auto majorKey() const { return currState().keys.majors; }

		[[nodiscard]] inline auto roughKeyAfter(Move move) const
		{
			assert(move);

			const auto &state = currState();

			const auto moving = state.boards.pieceAt(move.src());
			assert(moving != Piece::None);

			const auto captured = state.boards.pieceAt(move.dst());

			auto key = state.keys.all;

			key ^= keys::pieceSquare(moving, move.src());
			key ^= keys::pieceSquare(moving, move.dst());

			if (captured != Piece::None)
				key ^= keys::pieceSquare(captured, move.dst());

			key ^= keys::color();

			return key;
		}

		[[nodiscard]] inline auto allAttackersTo(Square square, Bitboard occupancy) const
		{
			assert(square != Square::None);

			const auto &bbs = this->bbs();

			Bitboard attackers{};

			const auto queens = bbs.queens();

			const auto rooks = queens | bbs.rooks();
			attackers |= rooks & attacks::getRookAttacks(square, occupancy);

			const auto bishops = queens | bbs.bishops();
			attackers |= bishops & attacks::getBishopAttacks(square, occupancy);

			attackers |= bbs.blackPawns() & attacks::getPawnAttacks(square, Color::White);
			attackers |= bbs.whitePawns() & attacks::getPawnAttacks(square, Color::Black);

			const auto knights = bbs.knights();
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = bbs.kings();
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		[[nodiscard]] inline auto attackersTo(Square square, Color attacker) const
		{
			assert(square != Square::None);

			const auto &bbs = this->bbs();

			Bitboard attackers{};

			const auto occ = bbs.occupancy();

			const auto queens = bbs.queens(attacker);

			const auto rooks = queens | bbs.rooks(attacker);
			attackers |= rooks & attacks::getRookAttacks(square, occ);

			const auto bishops = queens | bbs.bishops(attacker);
			attackers |= bishops & attacks::getBishopAttacks(square, occ);

			const auto pawns = bbs.pawns(attacker);
			attackers |= pawns & attacks::getPawnAttacks(square, oppColor(attacker));

			const auto knights = bbs.knights(attacker);
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = bbs.kings(attacker);
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		template <bool ThreatShortcut = true>
		[[nodiscard]] static inline auto isAttacked(const BoardState &state,
			Color toMove, Square square, Color attacker)
		{
			assert(toMove != Color::None);
			assert(square != Square::None);
			assert(attacker != Color::None);

			if constexpr (ThreatShortcut)
			{
				if (attacker != toMove)
					return state.threats[square];
			}

			const auto &bbs = state.boards.bbs();

			const auto occ = bbs.occupancy();

			if (const auto knights = bbs.knights(attacker);
				!(knights & attacks::getKnightAttacks(square)).empty())
				return true;

			if (const auto pawns = bbs.pawns(attacker);
				!(pawns & attacks::getPawnAttacks(square, oppColor(attacker))).empty())
				return true;

			if (const auto kings = bbs.kings(attacker);
				!(kings & attacks::getKingAttacks(square)).empty())
				return true;

			const auto queens = bbs.queens(attacker);

			if (const auto bishops = queens | bbs.bishops(attacker);
				!(bishops & attacks::getBishopAttacks(square, occ)).empty())
				return true;

			if (const auto rooks = queens | bbs.rooks(attacker);
				!(rooks & attacks::getRookAttacks(square, occ)).empty())
				return true;

			return false;
		}

		template <bool ThreatShortcut = true>
		[[nodiscard]] inline auto isAttacked(Square square, Color attacker) const
		{
			assert(square != Square::None);
			assert(attacker != Color::None);

			return isAttacked<ThreatShortcut>(currState(), toMove(), square, attacker);
		}

		[[nodiscard]] inline auto anyAttacked(Bitboard squares, Color attacker) const
		{
			assert(attacker != Color::None);

			if (attacker == opponent())
				return !(squares & currState().threats).empty();

			while (squares)
			{
				const auto square = squares.popLowestSquare();
				if (isAttacked(square, attacker))
					return true;
			}

			return false;
		}

		[[nodiscard]] inline auto kings() const { return currState().kings; }

		[[nodiscard]] inline auto blackKing() const { return currState().kings.black(); }
		[[nodiscard]] inline auto whiteKing() const { return currState().kings.white(); }

		template <Color C>
		[[nodiscard]] inline auto king() const
		{
			return currState().kings.color(C);
		}

		[[nodiscard]] inline auto king(Color c) const
		{
			assert(c != Color::None);
			return currState().kings.color(c);
		}

		template <Color C>
		[[nodiscard]] inline auto oppKing() const
		{
			return currState().kings.color(oppColor(C));
		}

		[[nodiscard]] inline auto oppKing(Color c) const
		{
			assert(c != Color::None);
			return currState().kings.color(oppColor(c));
		}

		[[nodiscard]] inline auto isCheck() const
		{
			return !currState().checkers.empty();
		}

		[[nodiscard]] inline auto checkers() const { return currState().checkers; }
		[[nodiscard]] inline auto pinned() const { return currState().pinned; }
		[[nodiscard]] inline auto threats() const { return currState().threats; }

		[[nodiscard]] auto hasCycle(i32 ply) const -> bool;
		[[nodiscard]] auto isDrawn(bool threefold) const -> bool;

		[[nodiscard]] inline auto captureTarget(Move move) const
		{
			assert(move != NullMove);

			const auto type = move.type();

			if (type == MoveType::Castling)
				return Piece::None;
			else if (type == MoveType::EnPassant)
				return flipPieceColor(boards().pieceAt(move.src()));
			else return boards().pieceAt(move.dst());
		}

		[[nodiscard]] inline auto isNoisy(Move move) const
		{
			assert(move != NullMove);

			const auto type = move.type();

			return type != MoveType::Castling
				&& (type == MoveType::EnPassant
					|| move.promo() == PieceType::Queen
					|| boards().pieceAt(move.dst()) != Piece::None);
		}

		[[nodiscard]] inline auto noisyCapturedPiece(Move move) const -> std::pair<bool, Piece>
		{
			assert(move != NullMove);

			const auto type = move.type();

			if (type == MoveType::Castling)
				return {false, Piece::None};
			else if (type == MoveType::EnPassant)
				return {true, colorPiece(PieceType::Pawn, toMove())};
			else
			{
				const auto captured = boards().pieceAt(move.dst());
				return {captured != Piece::None || move.promo() == PieceType::Queen, captured};
			}
		}

		[[nodiscard]] auto toFen() const -> std::string;

		[[nodiscard]] inline auto operator==(const Position &other) const
		{
			const auto &ourState = currState();
			const auto &theirState = other.m_states.back();

			// every other field is a function of these
			return ourState.boards == theirState.boards
				&& ourState.castlingRooks == theirState.castlingRooks
				&& ourState.enPassant == theirState.enPassant
				&& ourState.halfmove == theirState.halfmove
				&& m_fullmove == other.m_fullmove;
		}

		[[nodiscard]] inline auto deepEquals(const Position &other) const
		{
			return *this == other
				&& currState().kings == other.m_states.back().kings
				&& currState().checkers == other.m_states.back().checkers
				&& currState().pinned == other.m_states.back().pinned
				&& currState().threats == other.m_states.back().threats
				&& currState().keys == other.m_states.back().keys;
		}

		auto regen() -> void;

		[[nodiscard]] auto moveFromUci(const std::string &move) const -> Move;

		[[nodiscard]] inline auto plyFromStartpos() const -> u32
		{
			return m_fullmove * 2 - (m_blackToMove ? 0 : 1) - 1;
		}

		[[nodiscard]] inline auto classicalMaterial() const -> i32
		{
			const auto &bbs = currState().boards.bbs();

			return 1 * bbs.pawns().popcount()
				 + 3 * bbs.knights().popcount()
				 + 3 * bbs.bishops().popcount()
				 + 5 * bbs.rooks().popcount()
				 + 9 * bbs.queens().popcount();
		}

		auto operator=(const Position &) -> Position & = default;
		auto operator=(Position &&) -> Position & = default;

		[[nodiscard]] static auto starting() -> Position;
		[[nodiscard]] static auto fromFen(const std::string &fen) -> std::optional<Position>;
		[[nodiscard]] static auto fromFrcIndex(u32 n) -> std::optional<Position>;
		[[nodiscard]] static auto fromDfrcIndex(u32 n) -> std::optional<Position>;

	private:
		template <bool UpdateKeys = true>
		auto setPiece(Piece piece, Square square) -> void;
		template <bool UpdateKeys = true>
		auto removePiece(Piece piece, Square square) -> void;
		template <bool UpdateKeys = true>
		auto movePieceNoCap(Piece piece, Square src, Square dst) -> void;

		template <bool UpdateKeys = true, bool UpdateNnue = true>
		[[nodiscard]] auto movePiece(Piece piece, Square src, Square dst, eval::NnueUpdates &nnueUpdates) -> Piece;

		template <bool UpdateKeys = true, bool UpdateNnue = true>
		auto promotePawn(Piece pawn, Square src, Square dst, PieceType promo, eval::NnueUpdates &nnueUpdates) -> Piece;
		template <bool UpdateKeys = true, bool UpdateNnue = true>
		auto castle(Piece king, Square kingSrc, Square rookSrc, eval::NnueUpdates &nnueUpdates) -> void;
		template <bool UpdateKeys = true, bool UpdateNnue = true>
		auto enPassant(Piece pawn, Square src, Square dst, eval::NnueUpdates &nnueUpdates) -> Piece;

		[[nodiscard]] inline auto calcCheckers() const
		{
			const auto color = toMove();
			const auto &state = currState();

			return attackersTo(state.kings.color(color), oppColor(color));
		}

		[[nodiscard]] inline auto calcPinned() const
		{
			const auto color = toMove();
			const auto &state = currState();

			Bitboard pinned{};

			const auto king = state.kings.color(color);
			const auto opponent = oppColor(color);

			const auto &bbs = state.boards.bbs();

			const auto ourOcc = bbs.occupancy(color);
			const auto oppOcc = bbs.occupancy(opponent);

			const auto oppQueens = bbs.queens(opponent);

			auto potentialAttackers
				= attacks::getBishopAttacks(king, oppOcc) & (oppQueens | bbs.bishops(opponent))
				| attacks::  getRookAttacks(king, oppOcc) & (oppQueens | bbs.  rooks(opponent));

			while (potentialAttackers)
			{
				const auto potentialAttacker = potentialAttackers.popLowestSquare();
				const auto maybePinned = ourOcc & rayBetween(potentialAttacker, king);

				if (maybePinned.one())
					pinned |= maybePinned;
			}

			return pinned;
		}

		[[nodiscard]] inline auto calcThreats() const
		{
			const auto us = toMove();
			const auto them = oppColor(us);

			const auto &state = currState();
			const auto &bbs = state.boards.bbs();

			Bitboard threats{};

			const auto occ = bbs.occupancy();

			const auto queens = bbs.queens(them);

			auto rooks = queens | bbs.rooks(them);
			while (rooks)
			{
				const auto rook = rooks.popLowestSquare();
				threats |= attacks::getRookAttacks(rook, occ);
			}

			auto bishops = queens | bbs.bishops(them);
			while (bishops)
			{
				const auto bishop = bishops.popLowestSquare();
				threats |= attacks::getBishopAttacks(bishop, occ);
			}

			auto knights = bbs.knights(them);
			while (knights)
			{
				const auto knight = knights.popLowestSquare();
				threats |= attacks::getKnightAttacks(knight);
			}

			const auto pawns = bbs.pawns(them);
			if (them == Color::Black)
				threats |= pawns.shiftDownLeft() | pawns.shiftDownRight();
			else threats |= pawns.shiftUpLeft() | pawns.shiftUpRight();

			threats |= attacks::getKingAttacks(state.kings.color(them));

			return threats;
		}

		// Unsets ep squares if they are invalid (no pawn is able to capture)
		static void filterEp(BoardState &state, Color capturing);

		bool m_blackToMove{};

		u32 m_fullmove{1};

		std::vector<BoardState> m_states{};
		std::vector<u64> m_keys{};
	};

	template <bool UpdateNnue>
	HistoryGuard<UpdateNnue>::~HistoryGuard()
	{
		m_pos.popMove<UpdateNnue>(m_nnueState);
	}

	[[nodiscard]] auto squareFromString(const std::string &str) -> Square;
}
