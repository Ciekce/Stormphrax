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

#include "material.h"

namespace polaris::eval
{
	namespace pst
	{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}
		namespace
		{
			constexpr auto BonusTables = std::array {
				std::array { // pawns
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0),
					S(46, 118), S(42, 107), S(7, 94), S(66, 50), S(8, 76), S(63, 40), S(-72, 110), S(-97, 137),
					S(17, 42), S(8, 41), S(22, 10), S(37, -28), S(56, -39), S(83, -16), S(48, 14), S(23, 24),
					S(-10, 20), S(-2, 6), S(-8, -5), S(11, -33), S(11, -23), S(18, -18), S(10, 1), S(-6, 3),
					S(-18, 3), S(-17, 2), S(-10, -16), S(8, -31), S(3, -24), S(10, -19), S(3, -10), S(-14, -12),
					S(-24, 0), S(-25, 0), S(-18, -10), S(-21, 6), S(-17, 3), S(-1, -6), S(10, -12), S(-11, -16),
					S(-14, 4), S(-3, 2), S(-13, 4), S(3, -2), S(-6, 18), S(35, -1), S(39, -7), S(0, -13),
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)
				},
				std::array { // knights
					S(-142, -51), S(-77, -24), S(-81, 14), S(-16, -15), S(70, -20), S(-93, -5), S(-30, -41), S(-90, -88),
					S(-68, 1), S(-45, 16), S(43, -12), S(23, 12), S(25, 2), S(46, -13), S(13, -6), S(-8, -24),
					S(-53, 2), S(19, 0), S(20, 26), S(45, 19), S(59, 11), S(105, -1), S(46, -7), S(40, -24),
					S(-3, 9), S(9, 13), S(5, 35), S(48, 32), S(29, 32), S(64, 23), S(23, 14), S(37, -4),
					S(-14, 11), S(-2, 7), S(13, 33), S(16, 34), S(27, 35), S(21, 31), S(46, 8), S(3, 8),
					S(-28, -2), S(-13, 16), S(6, -2), S(9, 23), S(27, 21), S(18, 3), S(23, -7), S(-17, -1),
					S(-26, -26), S(-22, -4), S(-10, 1), S(11, 14), S(11, 6), S(14, -7), S(7, -18), S(-1, -26),
					S(-104, -10), S(-16, -18), S(-23, -7), S(-7, 2), S(5, 2), S(-4, -5), S(-13, -1), S(-15, -49)
				},
				std::array { // bishops
					S(-17, -4), S(-28, -5), S(-102, 11), S(-126, 17), S(-73, 6), S(-94, 6), S(-25, -2), S(-36, -7),
					S(-30, 5), S(-22, 10), S(-37, 6), S(-49, 2), S(3, 0), S(16, -4), S(-10, 7), S(-44, -4),
					S(-13, 2), S(15, 1), S(18, 3), S(18, 0), S(44, -6), S(65, -3), S(42, -1), S(28, -2),
					S(-18, 4), S(11, 5), S(6, 18), S(44, 14), S(24, 17), S(37, 5), S(11, -1), S(-4, 12),
					S(-5, -5), S(2, 8), S(8, 10), S(28, 18), S(29, 8), S(4, 6), S(5, 4), S(6, -7),
					S(-2, -5), S(19, -7), S(11, 13), S(11, 2), S(14, 9), S(24, 4), S(13, -1), S(10, -10),
					S(14, -29), S(15, -7), S(21, -21), S(5, 7), S(12, 6), S(24, -12), S(35, -15), S(8, -31),
					S(-6, -20), S(23, -12), S(-1, 10), S(0, 0), S(18, -6), S(-9, 9), S(8, -15), S(3, -19)
				},
				std::array { // rooks
					S(-8, 20), S(17, 10), S(-31, 27), S(14, 11), S(18, 12), S(4, 14), S(35, 2), S(41, 3),
					S(-17, 19), S(-8, 19), S(9, 15), S(29, 8), S(49, -8), S(56, -2), S(13, 10), S(19, 8),
					S(-33, 15), S(-1, 11), S(-8, 13), S(9, 7), S(9, 1), S(57, -12), S(78, -13), S(29, -8),
					S(-39, 14), S(-2, 0), S(-17, 16), S(-5, 6), S(-3, 2), S(21, 0), S(17, -5), S(1, 1),
					S(-43, 9), S(-32, 8), S(-27, 11), S(-19, 6), S(-1, -6), S(-6, -2), S(26, -13), S(-15, -6),
					S(-33, -3), S(-26, 2), S(-23, -4), S(-18, -6), S(-15, -2), S(11, -16), S(23, -18), S(-8, -22),
					S(-37, -7), S(-22, -8), S(-22, -5), S(-13, -3), S(-5, -11), S(10, -15), S(18, -22), S(-46, -12),
					S(-12, -3), S(-10, -6), S(-12, -1), S(3, -14), S(6, -17), S(8, -6), S(-13, -8), S(-5, -19)
				},
				std::array { // queens
					S(-28, -12), S(-16, 2), S(-14, 22), S(6, 12), S(71, -11), S(35, 10), S(59, -18), S(40, 13),
					S(-30, -14), S(-56, 18), S(-25, 30), S(-31, 51), S(-50, 86), S(38, 8), S(18, 7), S(51, -5),
					S(-5, -21), S(-18, -12), S(-4, -5), S(-14, 44), S(48, 16), S(77, 16), S(78, -15), S(58, 15),
					S(-35, 10), S(-18, 5), S(-27, 10), S(-27, 38), S(-11, 48), S(11, 50), S(4, 63), S(6, 46),
					S(-14, 0), S(-30, 14), S(-13, 0), S(-28, 44), S(-11, 17), S(-11, 29), S(7, 31), S(-4, 37),
					S(-21, -14), S(-8, -10), S(-8, -5), S(-1, -31), S(-1, -12), S(5, -12), S(12, -13), S(6, -12),
					S(-26, -32), S(-9, -25), S(2, -10), S(15, -70), S(15, -49), S(22, -52), S(3, -52), S(10, -56),
					S(-6, -37), S(-18, -24), S(-6, -32), S(4, 4), S(-3, -30), S(-26, -20), S(-18, -29), S(-30, -60)
				},
				std::array { // kings
					S(-3, -76), S(56, -36), S(72, -22), S(1, 0), S(-82, 17), S(-74, 44), S(40, 26), S(28, -16),
					S(78, -22), S(-3, 38), S(-37, 37), S(63, 21), S(-7, 39), S(-47, 63), S(-5, 45), S(-8, 21),
					S(-13, 16), S(18, 32), S(76, 23), S(5, 35), S(7, 41), S(68, 53), S(81, 48), S(-9, 19),
					S(-10, -1), S(-28, 38), S(-16, 43), S(-49, 52), S(-74, 57), S(-75, 57), S(-41, 47), S(-84, 21),
					S(-125, 11), S(-15, 11), S(-51, 42), S(-98, 59), S(-122, 64), S(-88, 48), S(-80, 30), S(-108, 13),
					S(6, -17), S(-15, 7), S(-55, 30), S(-81, 45), S(-65, 45), S(-66, 34), S(-27, 17), S(-39, -1),
					S(57, -43), S(27, -11), S(-17, 14), S(-47, 25), S(-45, 26), S(-26, 16), S(21, -6), S(30, -27),
					S(35, -78), S(74, -58), S(51, -33), S(-48, 2), S(13, -19), S(-25, -8), S(54, -43), S(47, -73)
				}
			};

			std::array<std::array<TaperedScore, 64>, 12> createPsts()
			{
				std::array<std::array<TaperedScore, 64>, 12> psts{};

				for (const auto piece : {
					BasePiece::Pawn, BasePiece::Knight,
					BasePiece::Bishop, BasePiece::Rook,
					BasePiece::Queen, BasePiece::King
				})
				{
					const auto value = pieceValue(piece);
					const auto &bonusTable = BonusTables[static_cast<i32>(piece)];

					auto &blackPst = psts[static_cast<i32>(piece) * 2    ];
					auto &whitePst = psts[static_cast<i32>(piece) * 2 + 1];

					for (i32 square = 0; square < 64; ++square)
					{
						blackPst[square] = -value - bonusTable[square       ];
						whitePst[square] =  value + bonusTable[square ^ 0x38];
					}
				}

				return psts;
			}
		}
#undef S

		const std::array<std::array<TaperedScore, 64>, 12> PieceSquareTables = createPsts();
	}
}
