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

#include "material.h"

namespace stormphrax::eval
{
	namespace pst
	{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}
		namespace
		{
			constexpr auto BonusTables = std::array {
				std::array { // pawns
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0),
					S(49, 127), S(44, 116), S(8, 102), S(70, 55), S(9, 83), S(68, 45), S(-75, 119), S(-102, 148),
					S(18, 45), S(9, 43), S(24, 10), S(39, -30), S(59, -42), S(89, -18), S(51, 14), S(24, 25),
					S(-11, 20), S(-2, 5), S(-9, -7), S(11, -36), S(11, -26), S(19, -20), S(10, -1), S(-7, 2),
					S(-20, 2), S(-18, 0), S(-10, -19), S(8, -34), S(3, -27), S(10, -23), S(3, -13), S(-15, -14),
					S(-26, -1), S(-26, -1), S(-19, -11), S(-23, 5), S(-18, 1), S(-2, -8), S(10, -14), S(-12, -18),
					S(-15, 4), S(-3, 1), S(-14, 3), S(3, -3), S(-7, 17), S(37, -2), S(41, -9), S(-1, -15),
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)
				},
				std::array { // knights
					S(-150, -54), S(-81, -26), S(-86, 15), S(-17, -16), S(76, -22), S(-98, -5), S(-33, -43), S(-95, -94),
					S(-72, 1), S(-48, 17), S(46, -13), S(24, 13), S(26, 2), S(49, -13), S(14, -7), S(-9, -26),
					S(-57, 2), S(20, 0), S(21, 28), S(48, 20), S(62, 12), S(111, -1), S(49, -7), S(43, -25),
					S(-4, 9), S(9, 14), S(5, 38), S(51, 34), S(31, 34), S(68, 25), S(24, 15), S(40, -4),
					S(-15, 12), S(-2, 8), S(14, 35), S(17, 36), S(28, 37), S(23, 33), S(49, 8), S(3, 9),
					S(-30, -2), S(-14, 17), S(6, -2), S(9, 24), S(28, 22), S(19, 4), S(24, -8), S(-18, -2),
					S(-28, -28), S(-23, -4), S(-11, 1), S(11, 14), S(11, 7), S(14, -7), S(7, -19), S(-1, -28),
					S(-111, -11), S(-17, -18), S(-25, -7), S(-8, 2), S(5, 1), S(-4, -6), S(-14, -1), S(-16, -51)
				},
				std::array { // bishops
					S(-18, -4), S(-29, -5), S(-108, 11), S(-134, 19), S(-77, 6), S(-99, 6), S(-26, -2), S(-38, -8),
					S(-32, 5), S(-23, 10), S(-39, 6), S(-52, 3), S(3, 0), S(17, -5), S(-11, 7), S(-47, -5),
					S(-14, 3), S(16, 1), S(19, 3), S(19, 0), S(46, -7), S(69, -3), S(45, -2), S(30, -2),
					S(-20, 4), S(12, 5), S(6, 19), S(47, 15), S(26, 18), S(39, 5), S(11, -2), S(-4, 13),
					S(-5, -5), S(2, 8), S(9, 10), S(30, 19), S(30, 8), S(4, 6), S(5, 4), S(7, -7),
					S(-2, -5), S(20, -8), S(11, 13), S(12, 2), S(15, 10), S(25, 5), S(14, -1), S(10, -11),
					S(15, -31), S(16, -7), S(22, -23), S(5, 8), S(13, 7), S(26, -13), S(37, -16), S(9, -33),
					S(-6, -22), S(24, -13), S(-1, 11), S(0, 0), S(19, -6), S(-9, 10), S(8, -16), S(3, -21)
				},
				std::array { // rooks
					S(-8, 21), S(17, 11), S(-33, 28), S(15, 11), S(19, 13), S(5, 15), S(36, 3), S(43, 3),
					S(-18, 20), S(-8, 20), S(10, 16), S(31, 8), S(52, -8), S(59, -2), S(14, 11), S(19, 8),
					S(-35, 16), S(-1, 12), S(-8, 14), S(10, 8), S(10, 1), S(60, -13), S(82, -14), S(31, -9),
					S(-41, 15), S(-2, 0), S(-18, 17), S(-5, 6), S(-3, 2), S(22, 1), S(18, -5), S(1, 1),
					S(-46, 10), S(-33, 9), S(-28, 12), S(-19, 7), S(-1, -6), S(-7, -2), S(27, -13), S(-16, -6),
					S(-35, -3), S(-27, 2), S(-24, -4), S(-19, -6), S(-15, -2), S(12, -17), S(25, -19), S(-8, -23),
					S(-39, -7), S(-24, -8), S(-23, -5), S(-13, -4), S(-5, -12), S(10, -16), S(19, -24), S(-49, -13),
					S(-13, -4), S(-10, -6), S(-12, -2), S(3, -15), S(6, -18), S(9, -6), S(-14, -9), S(-5, -20)
				},
				std::array { // queens
					S(-29, -13), S(-17, 2), S(-15, 23), S(7, 12), S(75, -12), S(37, 11), S(63, -19), S(43, 14),
					S(-31, -15), S(-60, 19), S(-26, 32), S(-33, 54), S(-53, 91), S(41, 9), S(20, 7), S(54, -5),
					S(-5, -22), S(-19, -12), S(-4, -5), S(-15, 47), S(51, 18), S(82, 17), S(83, -16), S(61, 17),
					S(-38, 10), S(-19, 5), S(-28, 11), S(-29, 40), S(-12, 50), S(11, 53), S(4, 67), S(6, 49),
					S(-15, 0), S(-32, 15), S(-14, 0), S(-29, 47), S(-12, 18), S(-12, 31), S(7, 33), S(-5, 40),
					S(-22, -15), S(-9, -10), S(-9, -5), S(-1, -33), S(-1, -13), S(5, -13), S(12, -13), S(6, -12),
					S(-28, -34), S(-10, -26), S(2, -10), S(16, -75), S(16, -52), S(23, -56), S(3, -56), S(10, -59),
					S(-6, -40), S(-19, -26), S(-7, -34), S(4, 5), S(-3, -32), S(-28, -21), S(-19, -31), S(-32, -64)
				},
				std::array { // kings
					S(-4, -81), S(59, -38), S(77, -24), S(0, 0), S(-87, 18), S(-78, 47), S(42, 28), S(30, -17),
					S(85, -24), S(-3, 40), S(-39, 39), S(67, 22), S(-7, 42), S(-50, 67), S(-5, 48), S(-9, 23),
					S(-14, 16), S(19, 33), S(81, 24), S(6, 37), S(8, 44), S(71, 56), S(85, 51), S(-9, 21),
					S(-13, 0), S(-31, 41), S(-19, 46), S(-52, 55), S(-79, 60), S(-80, 60), S(-44, 50), S(-90, 22),
					S(-134, 11), S(-16, 12), S(-55, 44), S(-104, 63), S(-130, 68), S(-94, 50), S(-85, 32), S(-115, 13),
					S(6, -18), S(-16, 7), S(-59, 32), S(-86, 48), S(-68, 48), S(-70, 36), S(-29, 18), S(-41, -1),
					S(60, -46), S(29, -12), S(-18, 15), S(-50, 26), S(-47, 27), S(-27, 17), S(22, -6), S(32, -28),
					S(37, -83), S(78, -62), S(54, -35), S(-51, 2), S(13, -20), S(-26, -9), S(58, -45), S(50, -78)
				}
			};

			auto createPsts() -> std::array<std::array<TaperedScore, 64>, 12>
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
