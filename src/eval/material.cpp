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
					S(45, 121), S(43, 110), S(6, 96), S(63, 52), S(7, 78), S(65, 42), S(-62, 111), S(-93, 139),
					S(11, 45), S(6, 43), S(20, 11), S(34, -29), S(57, -41), S(82, -17), S(53, 13), S(21, 25),
					S(-17, 22), S(-3, 6), S(-10, -5), S(9, -33), S(16, -26), S(15, -18), S(15, -1), S(-9, 3),
					S(-25, 5), S(-18, 2), S(-10, -16), S(6, -31), S(8, -28), S(7, -20), S(8, -13), S(-16, -12),
					S(-30, 2), S(-26, 0), S(-19, -10), S(-23, 6), S(-11, -1), S(-4, -7), S(16, -15), S(-12, -16),
					S(-18, 6), S(-3, 3), S(-12, 4), S(2, -2), S(4, 13), S(34, -1), S(46, -10), S(0, -13),
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)
				},
				std::array { // knights
					S(-144, -52), S(-81, -24), S(-84, 15), S(-17, -14), S(72, -21), S(-95, -4), S(-33, -41), S(-89, -91),
					S(-68, 2), S(-46, 17), S(42, -12), S(22, 13), S(23, 3), S(50, -13), S(13, -6), S(-10, -24),
					S(-55, 3), S(20, 1), S(20, 27), S(45, 19), S(59, 11), S(106, -1), S(50, -7), S(44, -24),
					S(-4, 10), S(8, 14), S(4, 36), S(48, 33), S(31, 32), S(65, 24), S(22, 15), S(38, -3),
					S(-15, 12), S(-2, 7), S(13, 34), S(16, 35), S(26, 37), S(21, 32), S(47, 8), S(2, 9),
					S(-28, -2), S(-14, 16), S(6, -2), S(8, 23), S(28, 21), S(18, 3), S(22, -7), S(-17, -1),
					S(-25, -28), S(-23, -3), S(-11, 0), S(11, 13), S(11, 6), S(16, -8), S(5, -18), S(-1, -28),
					S(-108, -11), S(-14, -19), S(-21, -8), S(-5, 1), S(8, 0), S(-3, -6), S(-12, -2), S(-16, -50)
				},
				std::array { // bishops
					S(-18, -4), S(-30, -5), S(-105, 11), S(-128, 17), S(-73, 5), S(-94, 5), S(-27, -1), S(-40, -7),
					S(-30, 5), S(-23, 10), S(-37, 6), S(-51, 3), S(2, 1), S(17, -4), S(-11, 7), S(-44, -4),
					S(-13, 3), S(17, 1), S(17, 3), S(18, 0), S(44, -6), S(65, -2), S(45, -2), S(30, -2),
					S(-18, 5), S(10, 6), S(7, 18), S(44, 15), S(27, 17), S(36, 5), S(10, -1), S(-5, 14),
					S(-5, -5), S(1, 8), S(8, 10), S(28, 19), S(30, 8), S(4, 6), S(4, 4), S(6, -6),
					S(-1, -6), S(19, -7), S(11, 12), S(11, 2), S(14, 9), S(24, 4), S(13, -1), S(9, -10),
					S(15, -30), S(15, -7), S(20, -22), S(5, 7), S(12, 6), S(25, -13), S(35, -15), S(9, -32),
					S(-5, -22), S(25, -13), S(2, 10), S(3, -1), S(20, -6), S(-8, 9), S(10, -16), S(2, -19)
				},
				std::array { // rooks
					S(-10, 20), S(14, 11), S(-34, 27), S(13, 11), S(14, 13), S(-1, 15), S(29, 3), S(39, 3),
					S(-19, 20), S(-11, 19), S(7, 15), S(29, 8), S(47, -7), S(53, -2), S(10, 10), S(13, 8),
					S(-34, 15), S(1, 11), S(-8, 13), S(8, 8), S(8, 2), S(62, -14), S(82, -15), S(32, -10),
					S(-39, 14), S(2, -1), S(-15, 16), S(-6, 7), S(1, 1), S(25, -1), S(22, -6), S(2, 0),
					S(-44, 9), S(-28, 8), S(-24, 11), S(-17, 7), S(4, -6), S(-4, -3), S(28, -13), S(-16, -5),
					S(-35, -2), S(-25, 2), S(-21, -3), S(-16, -6), S(-11, -3), S(11, -15), S(23, -17), S(-12, -21),
					S(-40, -6), S(-21, -8), S(-19, -5), S(-11, -3), S(-1, -12), S(10, -14), S(21, -23), S(-49, -11),
					S(-15, -5), S(-11, -6), S(-11, -1), S(4, -14), S(8, -17), S(6, -6), S(-11, -9), S(-8, -21)
				},
				std::array { // queens
					S(-28, -13), S(-17, 2), S(-16, 22), S(6, 12), S(72, -11), S(36, 10), S(61, -19), S(41, 13),
					S(-27, -15), S(-57, 18), S(-25, 30), S(-32, 51), S(-51, 87), S(41, 8), S(17, 8), S(51, -4),
					S(-4, -21), S(-19, -11), S(-6, -3), S(-17, 47), S(47, 18), S(80, 16), S(82, -17), S(61, 15),
					S(-36, 11), S(-18, 6), S(-28, 11), S(-25, 37), S(-10, 48), S(10, 52), S(3, 66), S(6, 50),
					S(-15, 1), S(-30, 14), S(-14, 0), S(-26, 41), S(-12, 19), S(-12, 30), S(7, 32), S(-5, 40),
					S(-22, -13), S(-9, -9), S(-10, -4), S(-2, -32), S(0, -12), S(5, -12), S(11, -13), S(5, -11),
					S(-26, -34), S(-10, -26), S(1, -9), S(15, -72), S(15, -50), S(22, -54), S(2, -54), S(10, -58),
					S(-5, -40), S(-17, -26), S(-5, -35), S(5, 4), S(-2, -32), S(-26, -21), S(-16, -30), S(-31, -62)
				},
				std::array { // kings
					S(-2, -78), S(59, -37), S(74, -23), S(2, -1), S(-84, 18), S(-78, 46), S(40, 27), S(27, -16),
					S(86, -25), S(-3, 38), S(-37, 38), S(65, 21), S(-7, 41), S(-49, 65), S(-6, 47), S(-7, 21),
					S(-11, 14), S(20, 31), S(79, 24), S(6, 36), S(7, 42), S(70, 54), S(81, 49), S(-8, 19),
					S(-10, -2), S(-29, 39), S(-17, 44), S(-50, 54), S(-75, 58), S(-79, 59), S(-43, 49), S(-87, 21),
					S(-127, 10), S(-16, 12), S(-54, 43), S(-101, 61), S(-126, 66), S(-93, 50), S(-84, 32), S(-112, 13),
					S(5, -17), S(-18, 7), S(-59, 32), S(-85, 47), S(-70, 47), S(-72, 36), S(-31, 18), S(-43, 0),
					S(56, -44), S(23, -10), S(-21, 15), S(-52, 27), S(-48, 27), S(-33, 19), S(19, -5), S(29, -28),
					S(36, -81), S(80, -62), S(55, -35), S(-50, 2), S(13, -19), S(-23, -9), S(58, -46), S(49, -76)
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
