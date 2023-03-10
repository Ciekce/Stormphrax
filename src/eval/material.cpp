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
			constexpr std::array BonusTables {
				std::array { // pawns
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0),
					S(30, 151), S(42, 111), S(4, 129), S(46, 74), S(8, 68), S(43, 62), S(-34, 110), S(-66, 143),
					S(5, 93), S(7, 98), S(24, 45), S(31, 7), S(40, 1), S(93, 15), S(59, 60), S(30, 67),
					S(-15, 43), S(-1, 30), S(-3, 8), S(7, -25), S(29, -28), S(23, -9), S(25, 15), S(18, 11),
					S(-22, 15), S(-13, 20), S(-9, -7), S(3, -25), S(9, -22), S(7, -8), S(13, 2), S(0, -5),
					S(-27, 19), S(-21, 12), S(-13, 0), S(-22, 25), S(-8, 22), S(-3, 4), S(22, 0), S(0, -7),
					S(-13, 24), S(-2, 24), S(-1, 17), S(-4, 30), S(5, 51), S(36, 23), S(55, 9), S(8, 8),
					S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)
				},
				std::array { // knights
					S(-155, -74), S(-84, -19), S(-41, 0), S(-29, 4), S(50, -8), S(-66, -15), S(-16, -31), S(-89, -98),
					S(-29, 6), S(-13, 15), S(15, 5), S(39, 11), S(39, -4), S(85, -17), S(10, 0), S(22, -16),
					S(-8, 13), S(20, 8), S(36, 31), S(54, 25), S(86, 12), S(112, -5), S(59, -7), S(57, -14),
					S(0, 23), S(15, 19), S(35, 37), S(71, 27), S(55, 25), S(80, 24), S(40, 13), S(58, 5),
					S(-9, 27), S(5, 17), S(25, 42), S(31, 34), S(44, 50), S(43, 33), S(54, 5), S(13, 24),
					S(-33, 8), S(-7, 16), S(14, -17), S(16, 29), S(38, 19), S(28, 3), S(27, -3), S(2, 5),
					S(-38, -9), S(-24, 9), S(-7, 7), S(17, 29), S(20, 4), S(14, 2), S(-5, 5), S(1, -24),
					S(-81, -22), S(-11, -11), S(-19, -4), S(4, -1), S(17, 14), S(8, -6), S(-15, 25), S(-38, -41)
				},
				std::array { // bishops
					S(-16, 9), S(-21, 14), S(-55, 18), S(-59, 18), S(-40, 10), S(-47, 4), S(0, -1), S(-30, -16),
					S(-13, 2), S(-15, 18), S(-7, 19), S(-14, 17), S(3, 6), S(17, 1), S(-10, 16), S(6, -14),
					S(3, 22), S(26, 19), S(18, 24), S(38, 12), S(38, 14), S(79, 19), S(54, 9), S(43, 11),
					S(-10, 25), S(15, 29), S(26, 25), S(53, 34), S(46, 28), S(40, 26), S(16, 8), S(-4, 29),
					S(3, 3), S(-4, 29), S(18, 22), S(44, 29), S(39, 27), S(12, 19), S(2, 33), S(22, -12),
					S(-2, 23), S(31, -5), S(19, 31), S(20, 0), S(21, 13), S(24, 33), S(27, 5), S(24, 1),
					S(22, -42), S(13, 42), S(33, -40), S(5, 32), S(17, 52), S(33, -12), S(36, 35), S(25, -55),
					S(6, -17), S(48, -31), S(15, 13), S(7, 7), S(21, 3), S(-4, 28), S(22, -11), S(16, -23)
				},
				std::array { // rooks
					S(15, 61), S(15, 61), S(11, 71), S(21, 61), S(37, 55), S(32, 59), S(41, 57), S(65, 49),
					S(-6, 63), S(-7, 73), S(13, 76), S(43, 58), S(33, 58), S(64, 47), S(63, 43), S(89, 31),
					S(-15, 61), S(20, 56), S(8, 60), S(26, 50), S(57, 37), S(82, 30), S(118, 29), S(86, 24),
					S(-16, 63), S(2, 55), S(-5, 64), S(4, 54), S(17, 38), S(37, 32), S(58, 35), S(44, 31),
					S(-23, 51), S(-18, 53), S(-19, 55), S(-8, 49), S(-5, 45), S(-4, 48), S(39, 29), S(15, 29),
					S(-25, 43), S(-17, 39), S(-15, 38), S(-11, 39), S(4, 32), S(19, 22), S(61, -4), S(26, 1),
					S(-26, 30), S(-18, 34), S(-3, 34), S(3, 28), S(10, 25), S(19, 23), S(41, 8), S(-5, 17),
					S(-5, 42), S(-1, 32), S(0, 40), S(15, 18), S(23, 12), S(18, 43), S(32, 20), S(-1, 31)
				},
				std::array { // queens
					S(0, 43), S(0, 52), S(31, 74), S(42, 85), S(63, 69), S(65, 71), S(71, 44), S(53, 40),
					S(16, 44), S(-11, 72), S(-8, 108), S(1, 107), S(12, 129), S(56, 79), S(35, 71), S(94, 55),
					S(24, 50), S(17, 51), S(22, 89), S(31, 98), S(55, 110), S(103, 99), S(104, 67), S(115, 37),
					S(3, 63), S(13, 74), S(10, 87), S(14, 108), S(21, 120), S(39, 114), S(52, 94), S(54, 74),
					S(-2, 131), S(8, 70), S(8, 89), S(22, 87), S(23, 86), S(33, 61), S(44, 59), S(46, 58),
					S(9, 36), S(7, 122), S(18, 66), S(36, -10), S(43, -4), S(50, 1), S(65, -18), S(54, -17),
					S(11, 37), S(10, 62), S(20, 97), S(52, -51), S(42, -3), S(56, -39), S(46, -17), S(47, -17),
					S(12, 23), S(6, 48), S(14, 47), S(16, 129), S(24, 14), S(11, 19), S(14, 19), S(5, 19)
				},
				std::array { // kings
					S(-56, -80), S(12, -48), S(10, -22), S(-18, -1), S(-47, 3), S(-27, 17), S(9, 31), S(9, -50),
					S(7, -22), S(-10, 30), S(-28, 41), S(-3, 41), S(-5, 54), S(-5, 70), S(-21, 72), S(-18, 34),
					S(-31, -1), S(15, 43), S(-19, 59), S(-32, 71), S(-18, 78), S(12, 78), S(17, 70), S(-18, 31),
					S(-36, -5), S(-44, 37), S(-53, 61), S(-80, 72), S(-82, 76), S(-67, 73), S(-53, 59), S(-69, 20),
					S(-62, -20), S(-45, 18), S(-88, 51), S(-125, 74), S(-137, 73), S(-110, 60), S(-101, 40), S(-117, 12),
					S(-27, -29), S(-18, 5), S(-87, 31), S(-112, 52), S(-109, 55), S(-109, 42), S(-57, 20), S(-79, 4),
					S(68, -45), S(9, -8), S(-13, 3), S(-53, 16), S(-62, 22), S(-41, 16), S(16, -7), S(30, -31),
					S(54, -92), S(104, -75), S(72, -38), S(-65, -18), S(11, -27), S(-28, -21), S(68, -55), S(62, -99)
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
