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

#include "types.h"

#include <cstdint>
#include <array>

#include "core.h"
#include "util/static_vector.h"
#include "opts.h"
#include "util/bitrange.h"
#include "position/boards.h"

namespace stormphrax
{
	enum class MoveType
	{
		Standard = 0,
		Promotion,
		Castling,
		EnPassant
	};

	namespace common
	{
		static constexpr u32    TypeBits = 2;
		static constexpr u32   PromoBits = 2;
		static constexpr u32 DstFileBits = 3;
		static constexpr u32 DstRankBits = 3;
		static constexpr u32 SrcFileBits = 3;
		static constexpr u32 SrcRankBits = 3;

		static constexpr u32    TypeOffset = 0;
		static constexpr u32   PromoOffset =      TypeOffset +      TypeBits;
		static constexpr u32 DstFileOffset =     PromoOffset +     PromoBits;
		static constexpr u32 DstRankOffset =   DstFileOffset +   DstFileBits;
		static constexpr u32 SrcFileOffset =   DstRankOffset +   DstRankBits;
		static constexpr u32 SrcRankOffset =   SrcFileOffset +   SrcFileBits;

		static constexpr u32 SquareBits = 6;

		static constexpr u32 DstOffset = common::DstFileOffset;
		static constexpr u32 SrcOffset = common::SrcFileOffset;
	}

	class PackedMove;

	class Move
	{
	public:
		constexpr Move() = default;
		constexpr ~Move() = default;

		[[nodiscard]] constexpr auto srcIdx() const
		{
			using namespace common;
			return util::getBits<SrcOffset, SquareBits>(m_move);
		}

		[[nodiscard]] constexpr auto src() const
		{
			return static_cast<Square>(srcIdx());
		}

		[[nodiscard]] constexpr auto srcRank() const -> i32
		{
			using namespace common;
			return util::getBits<SrcRankOffset, SrcRankBits>(m_move);
		}

		[[nodiscard]] constexpr auto srcFile() const -> i32
		{
			using namespace common;
			return util::getBits<SrcFileOffset, SrcFileBits>(m_move);
		}

		[[nodiscard]] constexpr auto dstIdx() const
		{
			using namespace common;
			return util::getBits<DstOffset, SquareBits>(m_move);
		}

		[[nodiscard]] constexpr auto dst() const
		{
			return static_cast<Square>(dstIdx());
		}

		[[nodiscard]] constexpr auto dstRank() const -> i32
		{
			using namespace common;
			return util::getBits<DstRankOffset, DstRankBits>(m_move);
		}

		[[nodiscard]] constexpr auto dstFile() const -> i32
		{
			using namespace common;
			return util::getBits<DstFileOffset, DstFileBits>(m_move);
		}

		[[nodiscard]] constexpr auto promoIdx() const
		{
			using namespace common;
			return util::getBits<PromoOffset, PromoBits>(m_move);
		}

		[[nodiscard]] constexpr auto promo() const
		{
			return static_cast<PieceType>(promoIdx() + 1);
		}

		[[nodiscard]] constexpr auto type() const
		{
			using namespace common;
			return static_cast<MoveType>(util::getBits<TypeOffset, TypeBits>(m_move));
		}

		[[nodiscard]] constexpr auto noisy() const
		{
			return util::getBits<NoisyOffset, NoisyBits>(m_move) != 0;
		}

		[[nodiscard]] constexpr auto moving() const
		{
			return static_cast<Piece>(util::getBits<MovingOffset, MovingBits>(m_move));
		}

		[[nodiscard]] constexpr auto capturedIdx() const
		{
			return util::getBits<CapturedOffset, CapturedBits>(m_move);
		}

		[[nodiscard]] constexpr auto captured() const
		{
			return static_cast<Piece>(capturedIdx());
		}

		[[nodiscard]] constexpr auto kingside() const
		{
			return util::getBits<KingsideOffset, KingsideBits>(m_move) != 0;
		}

		// returns the king's actual destination square for non-FRC
		// castling moves, otherwise just the move's destination
		// used to avoid inflating the history of the generally
		// bad moves of putting the king in a corner when castling
		[[nodiscard]] constexpr auto historyDst() const
		{
			return static_cast<Square>(util::getBits<HistoryDstOffset, HistoryDstBits>(m_move));
		}

		[[nodiscard]] constexpr auto isNull() const
		{
			return m_move == 0;
		}

		[[nodiscard]] explicit constexpr operator bool() const
		{
			return !isNull();
		}

		constexpr auto operator==(Move other) const
		{
			return m_move == other.m_move;
		}

		[[nodiscard]] inline auto pack() const -> PackedMove;

		[[nodiscard]] static constexpr auto standard(Piece moving, Square src, Square dst, Piece captured)
		{
			using namespace common;

			assert(src != Square::None);
			assert(dst != Square::None);

			assert(moving != Piece::None);

			assert(captured == Piece::None || pieceColor(moving) != pieceColor(captured));

			const bool noisy = captured != Piece::None;

			u32 move{};

			move = util::setBits<       SrcOffset>(move, static_cast<u32>(src));
			move = util::setBits<       DstOffset>(move, static_cast<u32>(dst));
			move = util::setBits<      TypeOffset>(move, static_cast<u32>(MoveType::Standard));
			move = util::setBits<     NoisyOffset>(move, static_cast<u32>(noisy));
			move = util::setBits<    MovingOffset>(move, static_cast<u32>(moving));
			move = util::setBits<  CapturedOffset>(move, static_cast<u32>(captured));
			move = util::setBits<HistoryDstOffset>(move, static_cast<u32>(dst));

			return Move{move};
		}

		[[nodiscard]] static constexpr auto promotion(Piece pawn,
			Square src, Square dst, Piece captured, PieceType promo)
		{
			using namespace common;

			assert(src != Square::None);
			assert(dst != Square::None);

			assert(pawn != Piece::None);

			assert(pieceType(pawn) == PieceType::Pawn);

			assert(captured == Piece::None || pieceColor(pawn) != pieceColor(captured));

			const bool noisy = promo == PieceType::Queen;

			u32 move{};

			move = util::setBits<       SrcOffset>(move, static_cast<u32>(src));
			move = util::setBits<       DstOffset>(move, static_cast<u32>(dst));
			move = util::setBits<     PromoOffset>(move, static_cast<u32>(promo) - 1);
			move = util::setBits<      TypeOffset>(move, static_cast<u32>(MoveType::Promotion));
			move = util::setBits<     NoisyOffset>(move, noisy);
			move = util::setBits<    MovingOffset>(move, static_cast<u32>(pawn));
			move = util::setBits<  CapturedOffset>(move, static_cast<u32>(captured));
			move = util::setBits<HistoryDstOffset>(move, static_cast<u32>(dst));

			const auto mv = Move{move};

			assert(mv.moving() == pawn);
			assert(mv.src() == src);
			assert(mv.dst() == dst);
			assert(mv.captured() == captured);
			assert(mv.promo() == promo);

			return mv;
		}

		[[nodiscard]] static constexpr auto castling(Piece king, Square src, Square dst, bool kingside)
		{
			using namespace common;

			assert(src != Square::None);
			assert(dst != Square::None);

			assert(king != Piece::None);

			assert(pieceType(king) == PieceType::King);

			assert(kingside == (squareFile(src) < squareFile(dst)));

			const auto historyDst = g_opts.chess960 ? dst
				: toSquare(squareRank(src), kingside ? 2 : 6);

			u32 move{};

			move = util::setBits<       SrcOffset>(move, static_cast<u32>(src));
			move = util::setBits<       DstOffset>(move, static_cast<u32>(dst));
			move = util::setBits<      TypeOffset>(move, static_cast<u32>(MoveType::Castling));
			move = util::setBits<    MovingOffset>(move, static_cast<u32>(king));
			move = util::setBits<  CapturedOffset>(move, static_cast<u32>(Piece::None));
			move = util::setBits<  KingsideOffset>(move, static_cast<u32>(kingside));
			move = util::setBits<HistoryDstOffset>(move, static_cast<u32>(historyDst));

			return Move{move};
		}

		[[nodiscard]] static constexpr auto enPassant(Piece pawn, Square src, Square dst)
		{
			using namespace common;

			assert(src != Square::None);
			assert(dst != Square::None);

			assert(pawn != Piece::None);

			assert(pieceType(pawn) == PieceType::Pawn);

			const auto captured = flipPieceColor(pawn);

			u32 move{};

			move = util::setBits<       SrcOffset>(move, static_cast<u32>(src));
			move = util::setBits<       DstOffset>(move, static_cast<u32>(dst));
			move = util::setBits<      TypeOffset>(move, static_cast<u32>(MoveType::EnPassant));
			move = util::setBits<     NoisyOffset>(move, 1);
			move = util::setBits<  CapturedOffset>(move, static_cast<u32>(captured));
			move = util::setBits<    MovingOffset>(move, static_cast<u32>(pawn));
			move = util::setBits<HistoryDstOffset>(move, static_cast<u32>(dst));

			return Move{move};
		}

	private:
		explicit constexpr Move(u32 move) : m_move{move} {}

		static constexpr u32      NoisyBits = 1;
		static constexpr u32     MovingBits = 4;
		static constexpr u32   CapturedBits = 4;
		static constexpr u32  KingsideBits = 1;
		static constexpr u32 HistoryDstBits = 6;

		static constexpr u32 NoisyOffset = common::SrcRankOffset + common::SrcRankBits;

		static constexpr u32     MovingOffset =     NoisyOffset +     NoisyBits;
		static constexpr u32   CapturedOffset =    MovingOffset +    MovingBits;
		static constexpr u32   KingsideOffset =  CapturedOffset +  CapturedBits;
		static constexpr u32 HistoryDstOffset =  KingsideOffset +  KingsideBits;

		static_assert(HistoryDstOffset + HistoryDstBits <= 32);

		u32 m_move{};

		friend class PackedMove;
	};

	constexpr Move NullMove{};

	class PackedMove
	{
	public:
		constexpr PackedMove() = default;
		constexpr ~PackedMove() = default;

		[[nodiscard]] constexpr auto data() const
		{
			return m_move;
		}

		[[nodiscard]] constexpr auto isNull() const
		{
			return m_move == 0;
		}

		[[nodiscard]] explicit constexpr operator bool() const
		{
			return !isNull();
		}

		constexpr auto operator==(PackedMove other) const
		{
			return m_move == other.m_move;
		}

		[[nodiscard]] constexpr auto unpack(const PositionBoards &boards) const
		{
			using namespace common;

			if (isNull())
				return NullMove;

			u32 move = m_move;

			const auto src = static_cast<Square>(util::getBits<SrcOffset, SquareBits>(m_move));
			const auto dst = static_cast<Square>(util::getBits<DstOffset, SquareBits>(m_move));

			const auto type = static_cast<MoveType>(util::getBits<TypeOffset, TypeBits>(m_move));

			const auto promoIdx = util::getBits<PromoOffset, PromoBits>(m_move);

			assert(type == MoveType::Promotion || promoIdx == 0);

			move = util::setBits<  SrcOffset>(move, static_cast<u32>(src));
			move = util::setBits<  DstOffset>(move, static_cast<u32>(dst));
			move = util::setBits<PromoOffset>(move, promoIdx);
			move = util::setBits< TypeOffset>(move, static_cast<u32>(type));

			const auto moving = boards.pieceAt(src);
			assert(moving != Piece::None);

			auto captured = Piece::None;

			if (type == MoveType::EnPassant)
				captured = flipPieceColor(moving);
			else if (type != MoveType::Castling)
				captured = boards.pieceAt(dst);

			auto historyDst = dst;

			if (type == MoveType::Castling)
			{
				const bool kingside = squareFile(src) < squareFile(dst);
				move = util::setBits<Move::KingsideOffset>(move, static_cast<u32>(kingside));

				historyDst = g_opts.chess960 ? dst
					: toSquare(squareRank(src), kingside ? 6 : 2);
			}

			move = util::setBits<Move::    MovingOffset>(move, static_cast<u32>(moving));
			move = util::setBits<Move::  CapturedOffset>(move, static_cast<u32>(captured));
			move = util::setBits<Move::HistoryDstOffset>(move, static_cast<u32>(historyDst));

			return Move{move};
		}

	private:
		explicit constexpr PackedMove(u16 move) : m_move{move} {}

		u16 m_move{};

		friend class Move;
	};

	constexpr PackedMove PackedNullMove{};

	auto Move::pack() const -> PackedMove
	{
		return PackedMove{static_cast<u16>(m_move & 0xFFFF)};
	}

	// assumed upper bound for number of possible moves is 218
	constexpr usize DefaultMoveListCapacity = 256;

	using MoveList = StaticVector<Move, DefaultMoveListCapacity>;
}
