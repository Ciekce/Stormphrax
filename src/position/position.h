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

namespace polaris
{
	struct BoardState
	{
		PositionBoards boards{};

		u64 key{};
		u64 pawnKey{};

		Bitboard checkers{};

		TaperedScore material{};
		Score phase{};

		CastlingRooks castlingRooks{};

		Move lastMove{NullMove};

		u16 halfmove{};

		Square enPassant{Square::None};

		std::array<Square, 2> kings{Square::None, Square::None};

		[[nodiscard]] inline auto blackKing() const
		{
			return kings[0];
		}

		[[nodiscard]] inline auto whiteKing() const
		{
			return kings[1];
		}

		[[nodiscard]] inline auto king(Color c) const
		{
			return kings[static_cast<i32>(c)];
		}

		[[nodiscard]] inline auto &king(Color c)
		{
			return kings[static_cast<i32>(c)];
		}
	};

	static_assert(sizeof(BoardState) == 112);

	[[nodiscard]] inline auto squareToString(Square square)
	{
		constexpr auto Files = std::array{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
		constexpr auto Ranks = std::array{'1', '2', '3', '4', '5', '6', '7', '8'};

		const auto s = static_cast<u32>(square);
		return std::string{Files[s % 8], Ranks[s / 8]};
	}

	class Position;

	class HistoryGuard
	{
	public:
		explicit HistoryGuard(Position &pos, bool legal) : m_pos{pos}, m_legal{legal} {}
		inline ~HistoryGuard();

		[[nodiscard]] explicit operator bool() const { return m_legal; }

	private:
		Position &m_pos;
		bool m_legal{};
	};

	class Position
	{
	public:
		explicit Position(bool init = true);
		~Position() = default;

		Position(const Position &) = default;
		Position(Position &&) = default;

		template <bool UpdateMaterial = true, bool StateHistory = true>
		auto applyMoveUnchecked(Move move, TTable *prefetchTt = nullptr) -> bool;

		template <bool UpdateMaterial = true>
		[[nodiscard]] inline auto applyMove(Move move, TTable *prefetchTt = nullptr)
		{
			return HistoryGuard{*this, applyMoveUnchecked<UpdateMaterial>(move, prefetchTt)};
		}

		auto popMove() -> void;

		[[nodiscard]] auto isPseudolegal(Move move) const -> bool;

	private:
		[[nodiscard]] inline auto currState() -> auto & { return m_states.back(); }
		[[nodiscard]] inline auto currState() const -> const auto & { return m_states.back(); }

	public:
		[[nodiscard]] inline auto boards() const -> const auto & { return currState().boards; }

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

		[[nodiscard]] inline auto material() const { return currState().material; }

		[[nodiscard]] inline auto halfmove() const { return currState().halfmove; }
		[[nodiscard]] inline auto fullmove() const { return m_fullmove; }

		[[nodiscard]] inline auto key() const { return currState().key; }
		[[nodiscard]] inline auto pawnKey() const { return currState().pawnKey; }

		[[nodiscard]] inline auto interpScore(TaperedScore score) const
		{
			const auto &state = currState();
			return (score.midgame() * state.phase + score.endgame() * (24 - state.phase)) / 24;
		}

		[[nodiscard]] inline auto allAttackersTo(Square square, Bitboard occupancy) const
		{
			const auto &boards = this->boards();

			Bitboard attackers{};

			const auto queens = boards.queens();

			const auto rooks = queens | boards.rooks();
			attackers |= rooks & attacks::getRookAttacks(square, occupancy);

			const auto bishops = queens | boards.bishops();
			attackers |= bishops & attacks::getBishopAttacks(square, occupancy);

			attackers |= boards.blackPawns() & attacks::getPawnAttacks(square, Color::White);
			attackers |= boards.whitePawns() & attacks::getPawnAttacks(square, Color::Black);

			const auto knights = boards.knights();
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = boards.kings();
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		[[nodiscard]] inline auto attackersTo(Square square, Color attacker) const
		{
			const auto &boards = this->boards();

			Bitboard attackers{};

			const auto occ = boards.occupancy();

			const auto queens = boards.queens(attacker);

			const auto rooks = queens | boards.rooks(attacker);
			attackers |= rooks & attacks::getRookAttacks(square, occ);

			const auto bishops = queens | boards.bishops(attacker);
			attackers |= bishops & attacks::getBishopAttacks(square, occ);

			const auto pawns = boards.pawns(attacker);
			attackers |= pawns & attacks::getPawnAttacks(square, oppColor(attacker));

			const auto knights = boards.knights(attacker);
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = boards.kings(attacker);
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		[[nodiscard]] inline auto isAttacked(const PositionBoards &boards, Square square, Color attacker) const
		{
			const auto occ = boards.occupancy();

			if (const auto knights = boards.knights(attacker);
				!(knights & attacks::getKnightAttacks(square)).empty())
				return true;

			if (const auto pawns = boards.pawns(attacker);
				!(pawns & attacks::getPawnAttacks(square, oppColor(attacker))).empty())
				return true;

			if (const auto kings = boards.kings(attacker);
				!(kings & attacks::getKingAttacks(square)).empty())
				return true;

			const auto queens = boards.queens(attacker);

			if (const auto bishops = queens | boards.bishops(attacker);
				!(bishops & attacks::getBishopAttacks(square, occ)).empty())
				return true;

			if (const auto rooks = queens | boards.rooks(attacker);
				!(rooks & attacks::getRookAttacks(square, occ)).empty())
				return true;

			return false;
		}

		[[nodiscard]] inline auto isAttacked(Square square, Color attacker) const
		{
			return isAttacked(boards(), square, attacker);
		}

		[[nodiscard]] inline auto anyAttacked(Bitboard squares, Color attacker) const
		{
			while (squares)
			{
				const auto square = squares.popLowestSquare();
				if (isAttacked(square, attacker))
					return true;
			}

			return false;
		}

		[[nodiscard]] inline auto blackKing() const { return currState().blackKing(); }
		[[nodiscard]] inline auto whiteKing() const { return currState().whiteKing(); }

		template <Color C>
		[[nodiscard]] inline auto king() const
		{
			return currState().king(C);
		}

		[[nodiscard]] inline auto king(Color c) const
		{
			return currState().king(c);
		}

		template <Color C>
		[[nodiscard]] inline auto oppKing() const
		{
			return currState().king(oppColor(C));
		}

		[[nodiscard]] inline auto oppKing(Color c) const
		{
			return currState().king(oppColor(c));
		}

		[[nodiscard]] inline bool isCheck() const
		{
			return !m_states.back().checkers.empty();
		}

		[[nodiscard]] inline auto checkers() const { return currState().checkers; }

		[[nodiscard]] inline auto isDrawn(bool threefold) const
		{
			// TODO handle mate
			if (m_states.back().halfmove >= 100)
				return true;

			const auto currKey = currState().key;

			i32 repetitionsLeft = threefold ? 2 : 1;

			for (i32 i = static_cast<i32>(m_hashes.size() - 1); i >= 0; --i)
			{
				if (m_hashes[i] == currKey
					&& --repetitionsLeft == 0)
					return true;
			}

			const auto &boards = this->boards();

			if (!boards.pawns().empty() || !boards.majors().empty())
				return false;

			// KK
			if (boards.nonPk().empty())
				return true;

			// KNK or KBK
			if ((boards.blackNonPk().empty() && boards.whiteNonPk() == boards.whiteMinors() && !boards.whiteMinors().multiple())
				|| (boards.whiteNonPk().empty() && boards.blackNonPk() == boards.blackMinors() && !boards.blackMinors().multiple()))
				return true;

			// KBKB OCB
			if ((boards.blackNonPk() == boards.blackBishops() && boards.whiteNonPk() == boards.whiteBishops())
				&& !boards.blackBishops().multiple() && !boards.whiteBishops().multiple()
				&& (boards.blackBishops() & boards::LightSquares).empty() != (boards.whiteBishops() & boards::LightSquares).empty())
				return true;

			return false;
		}

		[[nodiscard]] inline auto isLikelyDrawn() const
		{
			const auto &boards = this->boards();

			if (!boards.pawns().empty() || !boards.majors().empty())
				return false;

			// KNK or KNNK
			if ((boards.blackNonPk().empty() && boards.whiteNonPk() == boards.whiteKnights() && boards.whiteKnights().popcount() < 3)
				|| (boards.whiteNonPk().empty() && boards.blackNonPk() == boards.blackKnights() && boards.blackKnights().popcount() < 3))
				return true;

			if (!boards.nonPk().empty())
			{
				// KNKN or KNKB or KBKB (OCB handled in isDrawn())
				if (!boards.whiteMinors().multiple() && !boards.blackMinors().multiple())
					return true;

				// KBBKB
				if (boards.nonPk() == boards.bishops()
					&& (boards.whiteBishops().popcount() < 3 && !boards.blackBishops().multiple()
						|| boards.blackBishops().popcount() < 3 && !boards.whiteBishops().multiple()))
					return true;
			}

			return false;
		}

		[[nodiscard]] inline auto lastMove() const
		{
			return m_states.empty() ? NullMove : currState().lastMove;
		}

		[[nodiscard]] inline auto captureTarget(Move move) const
		{
			const auto type = move.type();

			if (type == MoveType::Castling)
				return Piece::None;
			else if (type == MoveType::EnPassant)
				return flipPieceColor(boards().pieceAt(move.src()));
			else return boards().pieceAt(move.dst());
		}

		[[nodiscard]] inline auto isNoisy(Move move) const
		{
			const auto type = move.type();

			return type != MoveType::Castling
				&& (type == MoveType::EnPassant
					|| move.target() == BasePiece::Queen
					|| boards().pieceAt(move.dst()) != Piece::None);
		}

		[[nodiscard]] inline auto noisyCapturedPiece(Move move) const -> std::pair<bool, Piece>
		{
			const auto type = move.type();

			if (type == MoveType::Castling)
				return {false, Piece::None};
			else if (type == MoveType::EnPassant)
				return {true, colorPiece(BasePiece::Pawn, toMove())};
			else
			{
				const auto captured = boards().pieceAt(move.dst());
				return {captured != Piece::None || move.target() == BasePiece::Queen, captured};
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
				&& currState().phase == other.m_states.back().phase
				&& currState().material == other.m_states.back().material
				&& currState().key == other.m_states.back().key
				&& currState().pawnKey == other.m_states.back().pawnKey;
		}

		auto regenMaterial() -> void;

		template <bool EnPassantFromMoves = false>
		auto regen() -> void;

#ifndef NDEBUG
		auto printHistory(Move last = NullMove) -> void;

		template <bool CheckMaterial = true, bool HasHistory = true>
		auto verify() -> bool;
#endif

		[[nodiscard]] auto moveFromUci(const std::string &move) const -> Move;

		auto operator=(const Position &) -> Position & = default;
		auto operator=(Position &&) -> Position & = default;

		[[nodiscard]] static auto starting() -> Position;
		[[nodiscard]] static auto fromFen(const std::string &fen) -> std::optional<Position>;

	private:
		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto setPiece(Piece piece, Square square) -> void;
		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto removePiece(Piece piece, Square square) -> void;
		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto movePieceNoCap(Piece piece, Square src, Square dst) -> void;

		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		[[nodiscard]] auto movePiece(Piece piece, Square src, Square dst) -> Piece;

		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto promotePawn(Piece pawn, Square src, Square dst, BasePiece target) -> Piece;
		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto castle(Piece king, Square kingSrc, Square rookSrc) -> void;
		template <bool UpdateKeys = true, bool UpdateMaterial = true>
		auto enPassant(Piece pawn, Square src, Square dst) -> Piece;

		[[nodiscard]] inline auto calcCheckers() const
		{
			const auto color = toMove();
			const auto &state = currState();

			return attackersTo(state.king(color), oppColor(color));
		}

		bool m_blackToMove{};

		u32 m_fullmove{1};

		std::vector<BoardState> m_states{};
		std::vector<u64> m_hashes{};
	};

	HistoryGuard::~HistoryGuard()
	{
		m_pos.popMove();
	}

	[[nodiscard]] auto squareFromString(const std::string &str) -> Square;
}
