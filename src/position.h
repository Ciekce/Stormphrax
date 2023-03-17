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

#include "types.h"

#include <string>
#include <array>
#include <vector>
#include <stack>
#include <optional>

#include "bitboard.h"
#include "move.h"
#include "attacks/attacks.h"

#include <iostream>

namespace polaris
{
	struct PreviousMove
	{
		u64 key;
		u64 pawnKey;
		TaperedScore material;
		Bitboard checkers;
		Move move;
		PositionFlags flags;
		u16 halfmove;
		Piece captured;
		Square enPassant;
	};

#ifdef NDEBUG
	static_assert(sizeof(PreviousMove) == 40);
#endif

	[[nodiscard]] constexpr auto squareToString(Square square)
	{
		constexpr std::array Files{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
		constexpr std::array Ranks{'1', '2', '3', '4', '5', '6', '7', '8'};

		const auto s = static_cast<u32>(square);
		return std::string{Files[s % 8], Ranks[s / 8]};
	}

	class Position;

	class HistoryGuard
	{
	public:
		explicit HistoryGuard(Position &pos) : m_pos{pos} {}
		~HistoryGuard();

	private:
		Position &m_pos;
	};

	class Position
	{
	public:
		explicit Position(bool init = true);
		~Position() = default;

		Position(const Position &) = default;
		Position(Position &&) = default;

		template <bool UpdateMaterial = true, bool History = true>
		void applyMoveUnchecked(Move move);

		template <bool UpdateMaterial = true>
		[[nodiscard]] inline HistoryGuard applyMove(Move move)
		{
			HistoryGuard guard{*this};
			applyMoveUnchecked<UpdateMaterial>(move);
			return guard;
		}

		void popMove();

		[[nodiscard]] bool isPseudolegal(Move move) const;

		[[nodiscard]] inline auto pieceAt(u32 rank, u32 file) const { return m_pieces[rank][file]; }
		[[nodiscard]] inline auto pieceAt(Square square) const { return pieceAt(squareRank(square), squareFile(square)); }

		[[nodiscard]] inline auto blackOccupancy() const { return m_blackPop; }
		[[nodiscard]] inline auto whiteOccupancy() const { return m_whitePop; }

		template <Color C>
		[[nodiscard]] inline auto occupancy() const
		{
			if constexpr(C == Color::Black)
				return m_blackPop;
			else return m_whitePop;
		}

		[[nodiscard]] inline auto occupancy(Color color) const { return color == Color::Black ? m_blackPop : m_whitePop; }

		[[nodiscard]] inline auto occupancy() const { return m_whitePop | m_blackPop; }

		[[nodiscard]] inline auto board(Piece piece) const { return m_boards[static_cast<i32>(piece)]; }
		[[nodiscard]] inline auto board(BasePiece piece, Color color) const { return board(colorPiece(piece, color)); }

		[[nodiscard]] inline auto blackPawns() const { return m_boards[static_cast<i32>(Piece::BlackPawn)]; }
		[[nodiscard]] inline auto whitePawns() const { return m_boards[static_cast<i32>(Piece::WhitePawn)]; }

		[[nodiscard]] inline auto blackKnights() const { return m_boards[static_cast<i32>(Piece::BlackKnight)]; }
		[[nodiscard]] inline auto whiteKnights() const { return m_boards[static_cast<i32>(Piece::WhiteKnight)]; }

		[[nodiscard]] inline auto blackBishops() const { return m_boards[static_cast<i32>(Piece::BlackBishop)]; }
		[[nodiscard]] inline auto whiteBishops() const { return m_boards[static_cast<i32>(Piece::WhiteBishop)]; }

		[[nodiscard]] inline auto blackRooks() const { return m_boards[static_cast<i32>(Piece::BlackRook)]; }
		[[nodiscard]] inline auto whiteRooks() const { return m_boards[static_cast<i32>(Piece::WhiteRook)]; }

		[[nodiscard]] inline auto blackQueens() const { return m_boards[static_cast<i32>(Piece::BlackQueen)]; }
		[[nodiscard]] inline auto whiteQueens() const { return m_boards[static_cast<i32>(Piece::WhiteQueen)]; }

		[[nodiscard]] inline auto blackKings() const { return m_boards[static_cast<i32>(Piece::BlackKing)]; }
		[[nodiscard]] inline auto whiteKings() const { return m_boards[static_cast<i32>(Piece::WhiteKing)]; }

		[[nodiscard]] inline auto blackMinors() const
		{
			return blackKnights() | blackBishops();
		}

		[[nodiscard]] inline auto whiteMinors() const
		{
			return whiteKnights() | whiteBishops();
		}

		[[nodiscard]] inline auto blackMajors() const
		{
			return blackRooks() | blackQueens();
		}

		[[nodiscard]] inline auto whiteMajors() const
		{
			return whiteRooks() | whiteQueens();
		}

		[[nodiscard]] inline auto blackNonPk() const
		{
			return blackMinors() | blackMajors();
		}

		[[nodiscard]] inline auto whiteNonPk() const
		{
			return whiteMinors() | whiteMajors();
		}

		template <Color C>
		[[nodiscard]] inline auto pawns() const
		{
			if constexpr(C == Color::Black)
				return blackPawns();
			else return whitePawns();
		}

		template <Color C>
		[[nodiscard]] inline auto knights() const
		{
			if constexpr(C == Color::Black)
				return blackKnights();
			else return whiteKnights();
		}

		template <Color C>
		[[nodiscard]] inline auto bishops() const
		{
			if constexpr(C == Color::Black)
				return blackBishops();
			else return whiteBishops();
		}

		template <Color C>
		[[nodiscard]] inline auto rooks() const
		{
			if constexpr(C == Color::Black)
				return blackRooks();
			else return whiteRooks();
		}

		template <Color C>
		[[nodiscard]] inline auto queens() const
		{
			if constexpr(C == Color::Black)
				return blackQueens();
			else return whiteQueens();
		}

		template <Color C>
		[[nodiscard]] inline auto kings() const
		{
			if constexpr(C == Color::Black)
				return blackKings();
			else return whiteKings();
		}

		template <Color C>
		[[nodiscard]] inline auto minors() const
		{
			if constexpr(C == Color::Black)
				return blackMinors();
			else return whiteMinors();
		}

		template <Color C>
		[[nodiscard]] inline auto majors() const
		{
			if constexpr(C == Color::Black)
				return blackMajors();
			else return whiteMajors();
		}

		template <Color C>
		[[nodiscard]] inline auto nonPk() const
		{
			if constexpr(C == Color::Black)
				return blackNonPk();
			else return whiteNonPk();
		}

		[[nodiscard]] inline auto pawns(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackPawn : Piece::WhitePawn)];
		}

		[[nodiscard]] inline auto knights(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackKnight : Piece::WhiteKnight)];
		}

		[[nodiscard]] inline auto bishops(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackBishop : Piece::WhiteBishop)];
		}

		[[nodiscard]] inline auto rooks(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackRook : Piece::WhiteRook)];
		}

		[[nodiscard]] inline auto queens(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackQueen : Piece::WhiteQueen)];
		}

		[[nodiscard]] inline auto kings(Color color) const
		{
			return m_boards[static_cast<i32>(color == Color::Black ? Piece::BlackKing : Piece::WhiteKing)];
		}

		[[nodiscard]] inline auto minors(Color color) const
		{
			return color == Color::Black ? blackMinors() : whiteMinors();
		}

		[[nodiscard]] inline auto majors(Color color) const
		{
			return color == Color::Black ? blackMajors() : whiteMajors();
		}

		[[nodiscard]] inline auto nonPk(Color color) const
		{
			return color == Color::Black ? blackNonPk() : whiteNonPk();
		}

		[[nodiscard]] inline const auto &pieces() const { return m_pieces; }

		[[nodiscard]] inline auto toMove() const
		{
			return testFlags(m_flags, PositionFlags::BlackToMove) ? Color::Black : Color::White;
		}

		[[nodiscard]] inline auto opponent() const
		{
			return testFlags(m_flags, PositionFlags::BlackToMove) ? Color::White : Color::Black;
		}

		[[nodiscard]] inline auto castling() const { return m_flags & PositionFlags::AllCastling; }
		[[nodiscard]] inline auto enPassant() const { return m_enPassant; }

		[[nodiscard]] inline auto material() const { return m_material; }

		[[nodiscard]] inline auto halfmove() const { return m_halfmove; }
		[[nodiscard]] inline auto fullmove() const { return m_fullmove; }

		[[nodiscard]] inline auto key() const { return m_key; }
		[[nodiscard]] inline auto pawnKey() const { return m_pawnKey; }

		[[nodiscard]] inline auto interpScore(TaperedScore score) const
		{
			return (score.midgame * m_phase + score.endgame * (24 - m_phase)) / 24;
		}

		[[nodiscard]] inline Bitboard allAttackersTo(Square square, Bitboard occupancy) const
		{
			Bitboard attackers{};

			const auto queens = blackQueens() | whiteQueens();

			const auto rooks = queens | blackRooks() | whiteRooks();
			attackers |= rooks & attacks::getRookAttacks(square, occupancy);

			const auto bishops = queens | blackBishops() | whiteBishops();
			attackers |= bishops & attacks::getBishopAttacks(square, occupancy);

			attackers |= blackPawns() & attacks::getPawnAttacks(square, Color::White);
			attackers |= whitePawns() & attacks::getPawnAttacks(square, Color::Black);

			const auto knights = blackKnights() | whiteKnights();
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = blackKings() | whiteKings();
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		[[nodiscard]] inline Bitboard attackersTo(Square square, Color attacker) const
		{
			Bitboard attackers{};

			const auto occupancy = m_blackPop | m_whitePop;

			const auto queens = this->queens(attacker);

			const auto rooks = queens | this->rooks(attacker);
			attackers |= rooks & attacks::getRookAttacks(square, occupancy);

			const auto bishops = queens | this->bishops(attacker);
			attackers |= bishops & attacks::getBishopAttacks(square, occupancy);

			const auto pawns = this->pawns(attacker);
			attackers |= pawns & attacks::getPawnAttacks(square, oppColor(attacker));

			const auto knights = this->knights(attacker);
			attackers |= knights & attacks::getKnightAttacks(square);

			const auto kings = this->kings(attacker);
			attackers |= kings & attacks::getKingAttacks(square);

			return attackers;
		}

		[[nodiscard]] inline bool isAttacked(Square square, Color attacker) const
		{
			const auto occupancy = m_blackPop | m_whitePop;

			if (const auto knights = this->knights(attacker);
				!(knights & attacks::getKnightAttacks(square)).empty())
				return true;

			if (const auto pawns = this->pawns(attacker);
				!(pawns & attacks::getPawnAttacks(square, oppColor(attacker))).empty())
				return true;

			if (const auto kings = this->kings(attacker);
				!(kings & attacks::getKingAttacks(square)).empty())
				return true;

			const auto queens = this->queens(attacker);

			if (const auto bishops = queens | this->bishops(attacker);
				!(bishops & attacks::getBishopAttacks(square, occupancy)).empty())
				return true;

			if (const auto rooks = queens | this->rooks(attacker);
				!(rooks & attacks::getRookAttacks(square, occupancy)).empty())
				return true;

			return false;
		}

		[[nodiscard]] inline auto blackKing() const { return m_blackKing; }
		[[nodiscard]] inline auto whiteKing() const { return m_whiteKing; }

		template <Color C>
		[[nodiscard]] inline auto king() const
		{
			if constexpr(C == Color::Black)
				return m_blackKing;
			else return m_whiteKing;
		}

		[[nodiscard]] inline auto king(Color c) const
		{
			return c == Color::Black ? m_blackKing : m_whiteKing;
		}

		template <Color C>
		[[nodiscard]] inline auto oppKing() const
		{
			if constexpr(C == Color::Black)
				return m_whiteKing;
			else return m_blackKing;
		}

		[[nodiscard]] inline auto oppKing(Color c) const
		{
			return c == Color::Black ? m_whiteKing : m_blackKing;
		}

		[[nodiscard]] inline bool isCheck() const
		{
			return !m_checkers.empty();
		}

		[[nodiscard]] inline auto checkers() const { return m_checkers; }

		[[nodiscard]] inline bool isDrawn() const
		{
			// TODO handle mate
			if (m_halfmove >= 100)
				return true;

			/*
			const auto last = static_cast<i32>(m_history.size() - 2);
			const auto lastIrreversible = std::max(0, last - m_halfmove);

		//	i32 repetitions = 1;

			for (i32 i = last; i >= lastIrreversible; i -= 2)
			{
				if (m_history[i].key == m_key
		//			&& ++repetitions == 3
					)
					return true;
			}
			 */

			i32 repetitions = 1;

			for (i32 i = static_cast<i32>(m_history.size() - 1); i >= 0; --i)
			{
				if (m_history[i].key == m_key
					&& ++repetitions == 3)
					return true;
			}

			if (!blackPawns().empty() || !whitePawns().empty()
				|| !blackMajors().empty() || !whiteMajors().empty())
				return false;

			// KK
			if (blackNonPk().empty() && whiteNonPk().empty())
				return true;

			// KNK or KBK
			if ((blackNonPk().empty() && whiteNonPk() == whiteMinors() && !whiteMinors().multiple())
				|| (whiteNonPk().empty() && blackNonPk() == blackMinors() && !blackMinors().multiple()))
				return true;

			// KBKB OCB
			if ((blackNonPk() == blackBishops() && whiteNonPk() == whiteBishops())
				&& !blackBishops().multiple() && !whiteBishops().multiple()
				&& (blackBishops() & boards::LightSquares).empty() != (whiteBishops() & boards::LightSquares).empty())
				return true;

			return false;
		}

		[[nodiscard]] inline bool isLikelyDrawn() const
		{
			// KNK or KNNK
			if ((blackNonPk().empty() && whiteNonPk() == whiteKnights() && whiteKnights().popcount() < 3)
				|| (whiteNonPk().empty() && blackNonPk() == blackKnights() && blackKnights().popcount() < 3))
				return true;

			if (!blackNonPk().empty() && !whiteNonPk().empty())
			{
				// KNKN or KNKB or KBKB (OCB handled in isDrawn())
				if ((whiteNonPk() ^ whiteMinors()).empty() && !whiteMinors().multiple()
					&& (blackNonPk() ^ blackMinors()).empty() && !blackMinors().multiple())
					return true;

				// KBBKB
				if (whiteNonPk() == whiteBishops() && blackNonPk() == blackBishops()
					&& (whiteBishops().popcount() < 3 && !blackBishops().multiple()
						|| blackBishops().popcount() < 3 && !whiteBishops().multiple()))
					return true;
			}

			return false;
		}

		[[nodiscard]] inline Move lastMove() const
		{
			return m_history.empty() ? NullMove : m_history.back().move;
		}

		[[nodiscard]] inline Piece captureTarget(Move move) const
		{
			const auto type = move.type();

			if (type == MoveType::Castling)
				return Piece::None;
			else if (type == MoveType::EnPassant)
				return flipPieceColor(pieceAt(move.src()));
			else return pieceAt(move.dst());
		}

		[[nodiscard]] inline bool isNoisy(Move move) const
		{
			const auto type = move.type();

			return type != MoveType::Castling
				&& (type == MoveType::EnPassant
					|| move.target() == BasePiece::Queen
					|| pieceAt(move.dst()) != Piece::None);
		}

		[[nodiscard]] std::string toFen() const;

		// for debugging
		[[maybe_unused]] void clearHistory();

		[[nodiscard]] inline bool operator==(const Position &other) const
		{
			// every other field is a function of these
			return m_boards == other.m_boards
				&& m_flags == other.m_flags
				&& m_enPassant == other.m_enPassant
				&& m_halfmove == other.m_halfmove
				&& m_fullmove == other.m_fullmove;
		}

		[[nodiscard]] inline bool deepEquals(const Position &other) const
		{
			return *this == other
				&& m_pieces == other.m_pieces
				&& m_blackPop == other.m_blackPop
				&& m_whitePop == other.m_whitePop
				&& m_blackKing == other.m_blackKing
				&& m_whiteKing == other.m_whiteKing
				&& m_checkers == other.m_checkers
				&& m_phase == other.m_phase
				&& m_material == other.m_material
				&& m_key == other.m_key
				&& m_pawnKey == other.m_pawnKey;
		}

		void regenMaterial();

		template <bool EnPassantFromMoves = false>
		void regen();

#ifndef NDEBUG
		void printHistory(Move last = NullMove);

		template <bool CheckMaterial = true, bool HasHistory = true>
		bool verify();
#endif

		[[nodiscard]] Move moveFromUci(const std::string &move) const;

		Position &operator=(const Position &) = default;
		Position &operator=(Position &&) = default;

		[[nodiscard]] static Position starting();
		[[nodiscard]] static std::optional<Position> fromFen(const std::string &fen);

	private:
		[[nodiscard]] inline auto &board(Piece piece) { return m_boards[static_cast<i32>(piece)]; }

		[[nodiscard]] inline auto &pieceAt(u32 rank, u32 file) { return m_pieces[rank][file]; }
		[[nodiscard]] inline auto &pieceAt(Square square) { return pieceAt(squareRank(square), squareFile(square)); }

		[[nodiscard]] inline auto &occupancy(Color color) { return color == Color::White ? m_whitePop : m_blackPop; }

		template <bool UpdateKey = true, bool UpdateMaterial = true>
		Piece setPiece(Square square, Piece piece);
		template <bool UpdateKey = true, bool UpdateMaterial = true>
		Piece removePiece(Square square);
		template <bool UpdateKey = true, bool UpdateMaterial = true>
		Piece movePiece(Square src, Square dst);

		template <bool UpdateKey = true, bool UpdateMaterial = true>
		Piece promotePawn(Square src, Square dst, BasePiece target);
		template <bool UpdateKey = true, bool UpdateMaterial = true>
		void castle(Square src, Square dst);
		template <bool UpdateKey = true, bool UpdateMaterial = true>
		Piece enPassant(Square src, Square dst);

		void unpromotePawn(Square src, Square dst, Piece captured);
		void uncastle(Square src, Square dst);
		void undoEnPassant(Square src, Square dst);

		[[nodiscard]] inline Bitboard calcCheckers() const
		{
			const auto c = toMove();
			return attackersTo(c == Color::White ? m_whiteKing : m_blackKing, oppColor(c));
		}

		std::array<Bitboard, 12> m_boards{};
		std::array<std::array<Piece, 8>, 8> m_pieces{};

		Bitboard m_blackPop{};
		Bitboard m_whitePop{};

		PositionFlags m_flags{};

		Square m_enPassant{Square::None};

		Square m_blackKing{Square::None};
		Square m_whiteKing{Square::None};

		Bitboard m_checkers{};

		Score m_phase{};
		TaperedScore m_material{};

		i32 m_halfmove{};
		i32 m_fullmove{1};

		u64 m_key{};
		u64 m_pawnKey{};

		std::vector<PreviousMove> m_history{};
	};

	[[nodiscard]] Square squareFromString(const std::string &str);
}
