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
					S(19, 103), S(28, 71), S(-2, 85), S(32, 41), S(1, 36), S(29, 31), S(-33, 70), S(-59, 97),
					S(-2, 56), S(0, 60), S(14, 17), S(19, -13), S(27, -18), S(70, -7), S(42, 29), S(19, 35),
					S(-18, 16), S(-6, 5), S(-8, -13), S(0, -39), S(18, -42), S(13, -26), S(15, -7), S(9, -10),
					S(-23, -7), S(-16, -3), S(-13, -25), S(-3, -39), S(2, -37), S(0, -26), S(5, -17), S(-6, -23),
					S(-27, -4), S(-23, -9), S(-16, -19), S(-23, 1), S(-12, -1), S(-8, -16), S(12, -19), S(-6, -25),
					S(-16, 0), S(-7, 0), S(-6, -5), S(-9, 5), S(-2, 22), S(24, 0), S(39, -12), S(1, -13),
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)
				},
				std::array { // knights
					S(-134, -63), S(-76, -19), S(-41, -3), S(-32, 0), S(32, -10), S(-62, -15), S(-21, -28), S(-80, -83),
					S(-32, 2), S(-19, 9), S(4, 1), S(23, 6), S(23, -7), S(61, -17), S(0, -3), S(10, -16),
					S(-15, 7), S(8, 3), S(21, 22), S(36, 17), S(61, 6), S(83, -7), S(40, -9), S(38, -15),
					S(-8, 15), S(4, 12), S(20, 27), S(49, 19), S(36, 17), S(57, 16), S(24, 7), S(39, 1),
					S(-15, 19), S(-4, 10), S(12, 31), S(17, 24), S(27, 37), S(27, 23), S(36, 1), S(2, 16),
					S(-35, 3), S(-14, 10), S(3, -17), S(5, 20), S(23, 12), S(15, -1), S(14, -6), S(-6, 1),
					S(-39, -11), S(-28, 4), S(-14, 2), S(6, 20), S(8, 0), S(3, -2), S(-12, 1), S(-7, -23),
					S(-74, -21), S(-17, -12), S(-23, -7), S(-5, -4), S(6, 8), S(-2, -8), S(-20, 17), S(-39, -37)
				},
				std::array { // bishops
					S(-23, -1), S(-27, 3), S(-55, 6), S(-58, 6), S(-42, -1), S(-48, -5), S(-10, -9), S(-34, -22),
					S(-21, -7), S(-22, 6), S(-16, 7), S(-21, 5), S(-8, -4), S(4, -8), S(-18, 4), S(-5, -20),
					S(-8, 9), S(11, 7), S(5, 11), S(21, 1), S(21, 3), S(54, 7), S(34, -1), S(25, 0),
					S(-18, 12), S(2, 15), S(11, 12), S(33, 19), S(27, 14), S(22, 12), S(3, -2), S(-13, 15),
					S(-8, -6), S(-13, 15), S(5, 9), S(26, 15), S(22, 13), S(0, 7), S(-8, 18), S(8, -18),
					S(-12, 10), S(15, -13), S(5, 16), S(6, -9), S(7, 2), S(9, 18), S(12, -5), S(9, -8),
					S(8, -43), S(0, 25), S(17, -41), S(-6, 17), S(4, 33), S(17, -18), S(19, 20), S(10, -53),
					S(-5, -22), S(29, -34), S(2, 2), S(-4, -3), S(7, -6), S(-13, 14), S(8, -18), S(3, -27)
				},
				std::array { // rooks
					S(-3, 16), S(-3, 16), S(-6, 24), S(2, 16), S(15, 11), S(11, 14), S(18, 13), S(38, 6),
					S(-20, 17), S(-20, 26), S(-4, 28), S(20, 13), S(12, 13), S(37, 5), S(36, 1), S(57, -8),
					S(-27, 16), S(1, 12), S(-8, 15), S(6, 7), S(31, -4), S(52, -9), S(81, -10), S(55, -14),
					S(-28, 17), S(-13, 11), S(-19, 18), S(-12, 10), S(-1, -3), S(15, -8), S(32, -5), S(21, -8),
					S(-33, 8), S(-29, 9), S(-30, 11), S(-21, 6), S(-19, 3), S(-18, 5), S(17, -10), S(-3, -10),
					S(-35, 1), S(-29, -2), S(-27, -3), S(-24, -2), S(-12, -8), S(1, -16), S(35, -37), S(6, -33),
					S(-36, -9), S(-29, -6), S(-17, -6), S(-12, -11), S(-7, -13), S(1, -15), S(18, -27), S(-19, -20),
					S(-19, 1), S(-16, -8), S(-15, -1), S(-3, -19), S(4, -24), S(0, 1), S(11, -17), S(-16, -8)
				},
				std::array { // queens
					S(-26, -12), S(-26, -4), S(-1, 13), S(8, 22), S(25, 9), S(27, 11), S(32, -11), S(17, -14),
					S(-13, -11), S(-35, 12), S(-32, 41), S(-25, 40), S(-16, 58), S(19, 17), S(2, 11), S(50, -2),
					S(-6, -6), S(-12, -5), S(-8, 25), S(-1, 33), S(19, 42), S(57, 34), S(58, 8), S(67, -17),
					S(-23, 4), S(-15, 13), S(-18, 24), S(-15, 41), S(-9, 51), S(6, 46), S(16, 30), S(18, 13),
					S(-27, 59), S(-19, 10), S(-19, 25), S(-8, 24), S(-7, 23), S(1, 3), S(10, 1), S(11, 0),
					S(-19, -17), S(-20, 52), S(-11, 7), S(3, -55), S(9, -50), S(15, -46), S(27, -61), S(18, -60),
					S(-17, -17), S(-18, 4), S(-10, 32), S(16, -88), S(8, -49), S(19, -78), S(11, -60), S(12, -60),
					S(-16, -28), S(-21, -8), S(-15, -9), S(-13, 58), S(-6, -35), S(-17, -31), S(-15, -31), S(-22, -31)
				},
				std::array { // kings
					S(-45, -65), S(10, -39), S(8, -18), S(-15, -1), S(-38, 2), S(-22, 14), S(7, 25), S(7, -40),
					S(6, -18), S(-8, 24), S(-23, 33), S(-2, 33), S(-4, 44), S(-4, 57), S(-17, 58), S(-15, 28),
					S(-25, -1), S(12, 35), S(-15, 48), S(-26, 57), S(-15, 63), S(10, 63), S(14, 57), S(-15, 25),
					S(-29, -4), S(-36, 30), S(-43, 49), S(-65, 58), S(-66, 61), S(-54, 59), S(-43, 48), S(-56, 16),
					S(-50, -16), S(-36, 15), S(-71, 41), S(-101, 60), S(-111, 59), S(-89, 49), S(-82, 32), S(-95, 10),
					S(-22, -23), S(-15, 4), S(-70, 25), S(-91, 42), S(-88, 45), S(-88, 34), S(-46, 16), S(-64, 3),
					S(55, -36), S(7, -6), S(-11, 2), S(-43, 13), S(-50, 18), S(-33, 13), S(13, -6), S(24, -25),
					S(44, -74), S(84, -61), S(58, -31), S(-53, -15), S(9, -22), S(-23, -17), S(55, -45), S(50, -80)
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
