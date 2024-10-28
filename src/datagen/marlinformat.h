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

#include <vector>

#include "format.h"
#include "../position/position.h"
#include "../util/u4array.h"

namespace stormphrax::datagen
{
	namespace marlinformat
	{
		// https://github.com/jnlt3/marlinflow/blob/main/marlinformat/src/lib.rs
		struct __attribute__((packed)) PackedBoard
		{
			u64 occupancy;
			util::U4Array<32> pieces;
			u8 stmEpSquare;
			u8 halfmoveClock;
			u16 fullmoveNumber;
			i16 eval;
			Outcome wdl;
			[[maybe_unused]] u8 extra;

			[[nodiscard]] static auto pack(const Position &pos, i16 score)
			{
				static constexpr u8 UnmovedRook = 6;

				PackedBoard board{};

				const auto castlingRooks = pos.castlingRooks();
				const auto &boards = pos.boards();

				auto occupancy = boards.bbs().occupancy();
				board.occupancy = occupancy;

				usize i = 0;
				while (occupancy)
				{
					const auto square = occupancy.popLowestSquare();
					const auto piece = boards.pieceAt(square);

					auto pieceId = piece.type().raw();

					if (piece.type() == piece_types::Rook
						&& (square == castlingRooks.black().kingside
							|| square == castlingRooks.black().queenside
							|| square == castlingRooks.white().kingside
							|| square == castlingRooks.white().queenside))
						pieceId = UnmovedRook;

					const u8 colorId = piece.color() == colors::Black ? (1 << 3) : 0;

					board.pieces[i++] = pieceId | colorId;
				}

				const u8 stm = pos.toMove() == colors::Black ? (1 << 7) : 0;

				const Square relativeEpSquare = pos.enPassant() == squares::None ? squares::None
					: pos.enPassant().withRank(pos.toMove() == colors::Black ? 2 : 5);

				board.stmEpSquare = stm | static_cast<u8>(relativeEpSquare.idx());
				board.halfmoveClock = pos.halfmove();
				board.fullmoveNumber = pos.fullmove();
				board.eval = score;

				return board;
			}
		};
	}

	class Marlinformat
	{
	public:
		Marlinformat();
		~Marlinformat() = default;

		static constexpr auto Extension = "bin";

		auto start(const Position &initialPosition) -> void;
		auto push(bool filtered, Move move, Score score) -> void;
		auto writeAllWithOutcome(std::ostream &stream, Outcome outcome) -> usize;

	private:
		std::vector<marlinformat::PackedBoard> m_positions{};
		Position m_curr;
	};

	static_assert(OutputFormat<Marlinformat>);
}
