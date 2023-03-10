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

#include "eval.h"

#include <array>
#include <iostream>

#include "../pretty.h"
#include "../attacks/attacks.h"
#include "../rays.h"

namespace polaris::eval
{
	namespace
	{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}

		// pawn structure
		constexpr auto DoubledPawn = S(-23, -25);
		// idea from weiss
		constexpr auto DoubledGappedPawn = S(-8, -12);
	//	constexpr auto TripledPawn = S(-17, -15);
		constexpr auto PawnDefender = S(18, 25);
		constexpr auto OpenPawn = S(-15, -11);
		constexpr auto BlockedPawn = S(-14, -13);

		constexpr std::array PawnPhalanx {
			S(0, 0), S(6, 2), S(25, 20), S(33, 36), S(61, 77), S(155, 225), S(139, 341)
		};

		constexpr std::array Passer {
			S(0, 0), S(-7, 21), S(-25, 37), S(-12, 73), S(13, 98), S(39, 140), S(166, 146)
		};

		constexpr std::array DefendedPasser {
			S(0, 0), S(0, 0), S(22, -27), S(13, -24), S(21, -10), S(49, 21), S(114, 34)
		};

		constexpr std::array BlockedPasser {
			S(0, 0), S(-15, 1), S(-5, 2), S(-11, -14), S(-15, -42), S(-13, -97), S(-115, -88)
		};

		constexpr std::array CandidatePasser {
			S(0, 0), S(2, 10), S(-6, 11), S(3, 28), S(21, 33), S(82, 87), S(264, 185)
		};

		constexpr auto DoubledPasser = S(35, -45);
		constexpr auto PasserHelper = S(-14, 25);

		// pawns
		constexpr auto PawnAttackingMinor = S(57, 16);
		constexpr auto PawnAttackingRook = S(68, -7);
		constexpr auto PawnAttackingQueen = S(50, -8);

		constexpr auto PasserSquareRule = S(-32, 147);

		// minors
		constexpr auto MinorBehindPawn = S(12, 18);

		constexpr auto MinorAttackingRook = S(49, -3);
		constexpr auto MinorAttackingQueen = S(41, 5);

		// knights
		constexpr auto KnightOutpost = S(33, 24);

		// bishops
		constexpr auto BishopPair = S(33, 82);

		// rooks
		constexpr auto RookOnOpenFile = S(31, 5);
		constexpr auto RookOnSemiOpenFile = S(6, -3);
		constexpr auto RookSupportingPasser = S(11, 9);
		constexpr auto RookAttackingQueen = S(56, -11);

		// queens

		// kings

		// mobility
		constexpr std::array KnightMobility {
			S(-13, 99), S(13, 98), S(27, 106), S(35, 111), S(43, 124), S(49, 138), S(58, 138), S(65, 135),
			S(72, 124)
		};

		constexpr std::array BishopMobility {
			S(-10, 110), S(5, 132), S(23, 98), S(34, 98), S(45, 103), S(51, 116), S(58, 123), S(61, 129),
			S(64, 133), S(70, 129), S(80, 125), S(87, 125), S(72, 141), S(64, 116)
		};

		constexpr std::array RookMobility {
			S(-33, 128), S(-11, 170), S(-4, 175), S(3, 170), S(6, 161), S(14, 161), S(20, 165), S(29, 171),
			S(37, 178), S(43, 184), S(48, 187), S(55, 192), S(63, 192), S(68, 188), S(50, 195)
		};

		constexpr std::array QueenMobility {
			S(-101, 73), S(-24, 166), S(-23, 326), S(-22, 427), S(-20, 429), S(-15, 416), S(-9, 403), S(-1, 362),
			S(9, 336), S(16, 322), S(21, 325), S(24, 337), S(28, 340), S(26, 359), S(27, 368), S(28, 377),
			S(26, 391), S(25, 398), S(32, 399), S(43, 398), S(42, 408), S(73, 396), S(60, 407), S(61, 403),
			S(26, 423), S(73, 346), S(-31, 291), S(-52, 272)
		};
#undef S

		template <Color Us>
		consteval std::array<Bitboard, 64> generateAntiPasserMasks()
		{
			std::array<Bitboard, 64> dst{};

			for (u32 i = 0; i < 64; ++i)
			{
				dst[i] = Bitboard::fromSquare(static_cast<Square>(i));
				dst[i] |= dst[i].shiftLeft() | dst[i].shiftRight();

				dst[i] = dst[i].template shiftUpRelative<Us>().template fillUpRelative<Us>();
			}

			return dst;
		}

		template <Color Us>
		constexpr auto AntiPasserMasks = generateAntiPasserMasks<Us>();

		template <Color Us>
		consteval std::array<Bitboard, 64> generatePawnHelperMasks()
		{
			std::array<Bitboard, 64> dst{};

			for (u32 i = 0; i < 64; ++i)
			{
				dst[i] = Bitboard::fromSquare(static_cast<Square>(i));
				dst[i] = dst[i].shiftLeft() | dst[i].shiftRight();

				dst[i] = dst[i].template shiftDownRelative<Us>().template fillDownRelative<Us>();
			}

			return dst;
		}

		template <Color Us>
		constexpr auto PawnHelperMasks = generatePawnHelperMasks<Us>();

		class Evaluator
		{
		public:
			explicit Evaluator(const Position &pos, PawnCache *pawnCache);
			~Evaluator() = default;

			[[nodiscard]] inline auto eval() const { return m_final; }
			void printEval() const;

		private:
			struct EvalData
			{
				Bitboard pawnAttacks;

				Bitboard semiOpen;
				Bitboard available;

				Bitboard passers;

				TaperedScore pawnStructure{};

				TaperedScore pawns{};
				TaperedScore knights{};
				TaperedScore bishops{};
				TaperedScore rooks{};
				TaperedScore queens{};
				TaperedScore kings{};

				TaperedScore mobility{};

				TaperedScore hanging{};
				TaperedScore pinned{};

				TaperedScore kingSafety{};
			};

			template <Color Us>
			void evalPawnStructure   (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalPawns           (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalKnights         (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalBishops         (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalRooks           (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalQueens          (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalKing            (const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalHangingAndPinned(const Position &pos, EvalData &ours, const EvalData &theirs);
			template <Color Us>
			void evalKingSafety      (const Position &pos, EvalData &ours, const EvalData &theirs);

			bool m_cachedPawnStructureEval{false};

			const Position &m_pos;

			EvalData m_blackData{};
			EvalData m_whiteData{};

			Bitboard m_openFiles{};

			TaperedScore m_total{};
			Score m_final;
		};

		Evaluator::Evaluator(const Position &pos, PawnCache *pawnCache)
			: m_pos{pos}
		{
			const auto blackPawns = pos.blackPawns();
			const auto whitePawns = pos.whitePawns();

			const auto blackPawnLeftAttacks = blackPawns.shiftDownLeft();
			const auto blackPawnRightAttacks = blackPawns.shiftDownRight();

			m_blackData.pawnAttacks = blackPawnLeftAttacks | blackPawnRightAttacks;

			const auto whitePawnLeftAttacks = whitePawns.shiftUpLeft();
			const auto whitePawnRightAttacks = whitePawns.shiftUpRight();

			m_whiteData.pawnAttacks = whitePawnLeftAttacks | whitePawnRightAttacks;

			m_blackData.semiOpen = ~whitePawns.fillFile();
			m_whiteData.semiOpen = ~blackPawns.fillFile();

			m_blackData.available = ~(pos.blackOccupancy() | m_whiteData.pawnAttacks);
			m_whiteData.available = ~(pos.whiteOccupancy() | m_blackData.pawnAttacks);

			m_openFiles = m_blackData.semiOpen & m_whiteData.semiOpen;

			m_total = pos.material();

			if (pawnCache)
			{
				auto &cacheEntry = pawnCache->probe(pos.pawnKey());

				if (cacheEntry.key == pos.pawnKey())
				{
					m_whiteData.pawnStructure = cacheEntry.eval;

					m_cachedPawnStructureEval = true;

					m_blackData.passers = cacheEntry.passers & pos.blackOccupancy();
					m_whiteData.passers = cacheEntry.passers & pos.whiteOccupancy();
				}
				else
				{
					evalPawnStructure<Color::Black>(pos, m_blackData, m_whiteData);
					evalPawnStructure<Color::White>(pos, m_whiteData, m_blackData);

					cacheEntry.key = pos.pawnKey();
					cacheEntry.eval = m_whiteData.pawnStructure - m_blackData.pawnStructure;
					cacheEntry.passers = m_blackData.passers | m_whiteData.passers;
				}
			}
			else
			{
				evalPawnStructure<Color::Black>(pos, m_blackData, m_whiteData);
				evalPawnStructure<Color::White>(pos, m_whiteData, m_blackData);
			}

			evalPawns<Color::Black>(pos, m_blackData, m_whiteData);
			evalPawns<Color::White>(pos, m_whiteData, m_blackData);

			evalKnights<Color::Black>(pos, m_blackData, m_whiteData);
			evalKnights<Color::White>(pos, m_whiteData, m_blackData);

			evalBishops<Color::Black>(pos, m_blackData, m_whiteData);
			evalBishops<Color::White>(pos, m_whiteData, m_blackData);

			evalRooks<Color::Black>(pos, m_blackData, m_whiteData);
			evalRooks<Color::White>(pos, m_whiteData, m_blackData);

			evalQueens<Color::Black>(pos, m_blackData, m_whiteData);
			evalQueens<Color::White>(pos, m_whiteData, m_blackData);

			evalKing<Color::Black>(pos, m_blackData, m_whiteData);
			evalKing<Color::White>(pos, m_whiteData, m_blackData);

			evalHangingAndPinned<Color::Black>(pos, m_blackData, m_whiteData);
			evalHangingAndPinned<Color::White>(pos, m_whiteData, m_blackData);

			evalKingSafety<Color::Black>(pos, m_blackData, m_whiteData);
			evalKingSafety<Color::White>(pos, m_whiteData, m_blackData);

			m_total += m_whiteData.pawnStructure - m_blackData.pawnStructure;

			m_total += m_whiteData.pawns         - m_blackData.pawns;
			m_total += m_whiteData.knights       - m_blackData.knights;
			m_total += m_whiteData.bishops       - m_blackData.bishops;
			m_total += m_whiteData.rooks         - m_blackData.rooks;
			m_total += m_whiteData.queens        - m_blackData.queens;
			m_total += m_whiteData.kings         - m_blackData.kings;

			m_total += m_whiteData.mobility      - m_blackData.mobility;

			m_total += m_whiteData.hanging       - m_blackData.hanging;
			m_total += m_whiteData.pinned        - m_blackData.pinned;

			m_total += m_whiteData.kingSafety    - m_blackData.kingSafety;

			m_final = pos.interpScore(m_total);

			if (pos.isLikelyDrawn())
				m_final /= 8;
		}

		void Evaluator::printEval() const
		{
			std::cout <<   "      Material: ";
			printScore(std::cout, m_pos.material());

			if (m_cachedPawnStructureEval)
			{
				std::cout << "\nPawn structure: ";
				printScore(std::cout, m_whiteData.pawnStructure);
				std::cout << " (cached)";
			}
			else
			{
				std::cout << "\nPawn structure: ";
				printScore(std::cout, m_whiteData.pawnStructure);
				std::cout << " - ";
				printScore(std::cout, m_blackData.pawnStructure);
			}

			std::cout << "\n         Pawns: ";
			printScore(std::cout, m_whiteData.pawns);
			std::cout << " - ";
			printScore(std::cout, m_blackData.pawns);

			std::cout << "\n       Knights: ";
			printScore(std::cout, m_whiteData.knights);
			std::cout << " - ";
			printScore(std::cout, m_blackData.knights);

			std::cout << "\n       Bishops: ";
			printScore(std::cout, m_whiteData.bishops);
			std::cout << " - ";
			printScore(std::cout, m_blackData.bishops);

			std::cout << "\n         Rooks: ";
			printScore(std::cout, m_whiteData.rooks);
			std::cout << " - ";
			printScore(std::cout, m_blackData.rooks);

			std::cout << "\n        Queens: ";
			printScore(std::cout, m_whiteData.queens);
			std::cout << " - ";
			printScore(std::cout, m_blackData.queens);

			std::cout << "\n         Kings: ";
			printScore(std::cout, m_whiteData.kings);
			std::cout << " - ";
			printScore(std::cout, m_blackData.kings);

			std::cout << "\n      Mobility: ";
			printScore(std::cout, m_whiteData.mobility);
			std::cout << " - ";
			printScore(std::cout, m_blackData.mobility);

			std::cout << "\n       Hanging: ";
			printScore(std::cout, m_whiteData.hanging);
			std::cout << " - ";
			printScore(std::cout, m_blackData.hanging);

			std::cout << "\n        Pinned: ";
			printScore(std::cout, m_whiteData.pinned);
			std::cout << " - ";
			printScore(std::cout, m_blackData.pinned);

			std::cout << "\n   King safety: ";
			printScore(std::cout, m_whiteData.kingSafety);
			std::cout << " - ";
			printScore(std::cout, m_blackData.kingSafety);

			std::cout << "\n      Total: ";
			printScore(std::cout, m_total);

			std::cout << "\n\nEval: ";
			printScore(std::cout, m_final);
			std::cout << "\n    with tempo bonus: ";
			printScore(std::cout, m_final + (m_pos.toMove() == Color::Black ? -Tempo : Tempo));

			std::cout << std::endl;
		}

		template <Color Us>
		void Evaluator::evalPawnStructure(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			constexpr auto Them = oppColor(Us);

			const auto ourPawns = pos.pawns<Us>();
			const auto theirPawns = pos.pawns<Them>();

			const auto up = ourPawns.template shiftUpRelative<Us>();

			const auto doubledPawns = up & ourPawns;
			ours.pawnStructure += DoubledPawn * doubledPawns.popcount();

			ours.pawnStructure += DoubledGappedPawn * (up.template shiftUpRelative<Us>() & ourPawns).popcount();
			ours.pawnStructure += PawnDefender * (ours.pawnAttacks & ourPawns).popcount();
			ours.pawnStructure += OpenPawn * (ourPawns & ours.semiOpen & ~ours.pawnAttacks).popcount();

			const auto blockedPawns = up & theirPawns;
			ours.pawnStructure += BlockedPawn * blockedPawns.popcount();

			auto phalanx = ourPawns & ourPawns.shiftLeft();
			while (!phalanx.empty())
			{
				const auto square = phalanx.popLowestSquare();
				const auto rank = relativeRank<Us>(squareRank(square));

				ours.pawnStructure += PawnPhalanx[rank];
			}

			auto pawns = ourPawns;
			while (!pawns.empty())
			{
				const auto square = pawns.popLowestSquare();
				const auto pawn = Bitboard::fromSquare(square);

				const auto rank = relativeRank<Us>(squareRank(square));

				const auto antiPassers = theirPawns & AntiPasserMasks<Us>[static_cast<i32>(square)];

				if (antiPassers.empty())
				{
					ours.pawnStructure += Passer[rank];

					if (!(pawn & ours.pawnAttacks).empty())
						ours.pawnStructure += DefendedPasser[rank];
					if (!(pawn & doubledPawns).empty())
						ours.pawnStructure += DoubledPasser;

					const auto helpers = ourPawns & PawnHelperMasks<Us>[static_cast<i32>(square)];
					ours.pawnStructure += PasserHelper * helpers.popcount();

					ours.passers |= pawn;
				}
				else if (!(pawn & ours.semiOpen).empty())
				{
					const auto stop = pawn.template shiftUpRelative<Us>();

					const auto levers = antiPassers
						& (pawn.template shiftUpLeftRelative<Us>() | pawn.template shiftUpRightRelative<Us>());

					if (antiPassers == levers)
						ours.pawnStructure += CandidatePasser[rank];
					else
					{
						const auto telelevers = antiPassers
							& (stop.template shiftUpLeftRelative<Us>() | stop.template shiftUpRightRelative<Us>());
						const auto helpers = ourPawns & (pawn.shiftLeft() | pawn.shiftRight());

						if (antiPassers == telelevers || telelevers.popcount() <= helpers.popcount())
							ours.pawnStructure += CandidatePasser[rank];
					}
				}
			}
		}

		template <Color Us>
		void Evaluator::evalPawns(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			constexpr auto Them = oppColor(Us);

			ours.pawns += PawnAttackingMinor * (ours.pawnAttacks & pos.template minors<Them>()).popcount();
			ours.pawns += PawnAttackingRook  * (ours.pawnAttacks & pos.template  rooks<Them>()).popcount();
			ours.pawns += PawnAttackingQueen * (ours.pawnAttacks & pos.template queens<Them>()).popcount();

			auto passers = ours.passers;
			while (!passers.empty())
			{
				const auto square = passers.popLowestSquare();
				const auto passer = Bitboard::fromSquare(square);

				const auto rank = relativeRank<Us>(squareRank(square));

				const auto promotion = toSquare(relativeRank<Us>(7), squareFile(square));

				if (pos.template nonPk<Them>().empty()
					&& (std::min(5, chebyshev(square, promotion)) + (Us == pos.toMove() ? 1 : 0))
						< chebyshev(pos.template king<Them>(), promotion))
					ours.pawns += PasserSquareRule;

				if (!(passer.template shiftUpRelative<Us>() & pos.occupancy()).empty())
					ours.pawns += BlockedPasser[rank];
			}
		}

		template <Color Us>
		void Evaluator::evalKnights(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			constexpr auto Them = oppColor(Us);

			auto knights = pos.knights<Us>();

			if (knights.empty())
				return;

			ours.knights += MinorBehindPawn
				* (knights.template shiftUpRelative<Us>() & pos.template pawns<Us>()).popcount();

			while (!knights.empty())
			{
				const auto square = knights.popLowestSquare();
				const auto knight = Bitboard::fromSquare(square);

				if ((AntiPasserMasks<Us>[static_cast<i32>(square)]
					& ~boards::Files[squareFile(square)]
					& pos.template pawns<Them>()).empty() && !(knight & ours.pawnAttacks).empty())
					ours.knights += KnightOutpost;

				const auto attacks = attacks::getKnightAttacks(square);

				ours.knights += MinorAttackingRook  * (attacks & pos.template  rooks<Them>()).popcount();
				ours.knights += MinorAttackingQueen * (attacks & pos.template queens<Them>()).popcount();

				ours.mobility += KnightMobility[(attacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		void Evaluator::evalBishops(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			constexpr auto Them = oppColor(Us);

			auto bishops = pos.bishops<Us>();

			if (bishops.empty())
				return;

			ours.bishops += MinorBehindPawn
				* (bishops.template shiftUpRelative<Us>() & pos.template pawns<Us>()).popcount();

			if (!(bishops & boards::DarkSquares).empty()
				&& !(bishops & boards::LightSquares).empty())
				ours.bishops += BishopPair;

			const auto occupancy = pos.occupancy();
			const auto xrayOcc = occupancy ^ pos.template bishops<Us>() ^ pos.template queens<Us>();

			while (!bishops.empty())
			{
				const auto square = bishops.popLowestSquare();

				const auto attacks = attacks::getBishopAttacks(square, occupancy);

				ours.bishops += MinorAttackingRook  * (attacks & pos.template  rooks<Them>()).popcount();
				ours.bishops += MinorAttackingQueen * (attacks & pos.template queens<Them>()).popcount();

				const auto mobilityAttacks = attacks::getBishopAttacks(square, xrayOcc);
				ours.mobility += BishopMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		void Evaluator::evalRooks(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			constexpr auto Them = oppColor(Us);

			auto rooks = pos.rooks<Us>();

			if (rooks.empty())
				return;

			const auto occupancy = pos.occupancy();
			const auto xrayOcc = occupancy ^ pos.template rooks<Us>() ^ pos.template queens<Us>();

			while (!rooks.empty())
			{
				const auto square = rooks.lowestSquare();
				const auto rook = rooks.popLowestBit();

				if (!(rook & m_openFiles).empty())
					ours.rooks += RookOnOpenFile;
				else if (!(rook & ours.semiOpen).empty())
					ours.rooks += RookOnSemiOpenFile;

				if (!(rook.template fillUpRelative<Us>() & ours.passers).empty())
					ours.rooks += RookSupportingPasser;

				const auto attacks = attacks::getRookAttacks(square, occupancy);

				ours.rooks += RookAttackingQueen * (attacks & pos.template queens<Them>()).popcount();

				const auto mobilityAttacks = attacks::getRookAttacks(square, xrayOcc);
				ours.mobility += RookMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		void Evaluator::evalQueens(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			auto queens = pos.queens<Us>();

			if (queens.empty())
				return;

			const auto occupancy = pos.occupancy();
			const auto xrayOcc = occupancy ^ pos.template bishops<Us>()
			    ^ pos.template rooks<Us>() ^ pos.template queens<Us>();

			while (!queens.empty())
			{
				const auto square = queens.popLowestSquare();

				const auto mobilityAttacks = attacks::getQueenAttacks(square, xrayOcc);
				ours.mobility += QueenMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		void Evaluator::evalKing(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			//TODO
		}

		template <Color Us>
		void Evaluator::evalHangingAndPinned(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			//TODO
		}

		template <Color Us>
		void Evaluator::evalKingSafety(const Position &pos, EvalData &ours, const EvalData &theirs)
		{
			//TODO
		}
	}

	Score staticEval(const Position &pos, PawnCache *pawnCache)
	{
		return Evaluator{pos, pawnCache}.eval() * colorScore(pos.toMove()) + Tempo;
		//return pos.interpScore(pos.material()) * colorScore(pos.toMove()) + Tempo;
	}

	void printEval(const Position &pos, PawnCache *pawnCache)
	{
		Evaluator{pos, pawnCache}.printEval();
	}
}
