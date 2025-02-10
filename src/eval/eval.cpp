/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include "eval.h"

#include <array>
#include <iostream>

#include "../pretty.h"
#include "../attacks/attacks.h"
#include "../rays.h"

namespace stormphrax::eval
{
	namespace
	{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}

		// pawn structure
		constexpr auto DoubledPawn = S(-19, -21);
		constexpr auto PawnDefender = S(18, 18);
		constexpr auto OpenPawn = S(-12, -5);

		constexpr auto PawnPhalanx = std::array {
			S(0, 0), S(3, 8), S(23, 12), S(27, 29), S(47, 66), S(126, 145), S(24, 275)
		};

		constexpr auto Passer = std::array {
			S(0, 0), S(1, 12), S(-4, 19), S(-14, 52), S(13, 74), S(8, 150), S(51, 162)
		};

		constexpr auto DefendedPasser = std::array {
			S(0, 0), S(0, 0), S(4, -9), S(5, -11), S(8, 0), S(35, 17), S(163, -12)
		};

		constexpr auto BlockedPasser = std::array {
			S(0, 0), S(-10, -3), S(-9, 4), S(-5, -9), S(-14, -26), S(5, -93), S(30, -146)
		};

		constexpr auto CandidatePasser = std::array {
			S(0, 0), S(8, -1), S(1, 2), S(3, 16), S(22, 19), S(49, 65), S(0, 0)
		};

		constexpr auto DoubledPasser = S(18, -31);
		constexpr auto PasserHelper = S(-8, 14);

		// pawns
		constexpr auto PawnAttackingMinor = S(55, 18);
		constexpr auto PawnAttackingRook = S(104, -33);
		constexpr auto PawnAttackingQueen = S(60, -17);

		constexpr auto PasserSquareRule = S(13, 108);

		// minors
		constexpr auto MinorBehindPawn = S(6, 19);

		constexpr auto MinorAttackingRook = S(42, 0);
		constexpr auto MinorAttackingQueen = S(29, 2);

		// knights
		constexpr auto KnightOutpost = S(27, 17);

		// bishops
		constexpr auto BishopPair = S(28, 62);

		// rooks
		constexpr auto RookOnOpenFile = S(43, 1);
		constexpr auto RookOnSemiOpenFile = S(16, 8);
		constexpr auto RookSupportingPasser = S(18, 15);
		constexpr auto RookAttackingQueen = S(59, -24);

		// queens

		// kings
		constexpr auto KingOnOpenFile = S(-75, 1);
		constexpr auto KingOnSemiOpenFile = S(-32, 18);

		// mobility
		constexpr auto KnightMobility = std::array {
			S(-44, -13), S(-24, -8), S(-13, -5), S(-8, 0), S(3, 3), S(9, 12), S(17, 10), S(22, 9),
			S(38, -9)
		};

		constexpr auto BishopMobility = std::array {
			S(-56, 4), S(-41, -14), S(-27, -24), S(-19, -17), S(-10, -8), S(-5, 1), S(0, 7), S(3, 9),
			S(2, 14), S(12, 10), S(23, 4), S(49, 0), S(7, 25), S(62, -11)
		};

		constexpr auto RookMobility = std::array {
			S(-44, -40), S(-31, -16), S(-24, -16), S(-19, -11), S(-18, -7), S(-12, -4), S(-9, 2), S(-4, 5),
			S(5, 7), S(12, 10), S(15, 13), S(25, 15), S(26, 19), S(45, 11), S(36, 12)
		};

		constexpr auto QueenMobility = std::array {
			S(-33, 67), S(-33, 235), S(-34, 95), S(-35, 56), S(-33, 52), S(-26, -24), S(-21, -62), S(-18, -72),
			S(-15, -70), S(-9, -78), S(-7, -63), S(-4, -52), S(-5, -48), S(4, -42), S(5, -31), S(1, -15),
			S(0, -4), S(17, -19), S(12, -5), S(29, -9), S(35, -5), S(68, -20), S(47, -4), S(88, -13),
			S(37, 4), S(44, 1), S(-45, 66), S(-70, 61)
		};
#undef S

		template <Color Us>
		constexpr auto AntiPasserMasks = []
		{
			std::array<Bitboard, 64> dst{};

			for (u32 i = 0; i < 64; ++i)
			{
				dst[i] = Bitboard::fromSquare(static_cast<Square>(i));
				dst[i] |= dst[i].shiftLeft() | dst[i].shiftRight();

				dst[i] = dst[i].template shiftUpRelative<Us>().template fillUpRelative<Us>();
			}

			return dst;
		}();

		template <Color Us>
		constexpr auto PawnHelperMasks = []
		{
			std::array<Bitboard, 64> dst{};

			for (u32 i = 0; i < 64; ++i)
			{
				dst[i] = Bitboard::fromSquare(static_cast<Square>(i));
				dst[i] = dst[i].shiftLeft() | dst[i].shiftRight();

				dst[i] = dst[i].template shiftDownRelative<Us>().template fillDownRelative<Us>();
			}

			return dst;
		}();

		[[nodiscard]] constexpr auto chebyshev(Square s1, Square s2)
		{
			const auto x1 = squareFile(s1);
			const auto x2 = squareFile(s2);

			const auto y1 = squareRank(s1);
			const auto y2 = squareRank(s2);

			return std::max(util::abs(x2 - x1), util::abs(y2 - y1));
		}

		[[nodiscard]] inline auto isLikelyDrawn(const Position &pos)
		{
			const auto &bbs = pos.bbs();

			if (!bbs.pawns().empty() || !bbs.majors().empty())
				return false;

			// KNK or KNNK
			if ((bbs.blackNonPk().empty() && bbs.whiteNonPk() == bbs.whiteKnights() && bbs.whiteKnights().popcount() < 3)
				|| (bbs.whiteNonPk().empty() && bbs.blackNonPk() == bbs.blackKnights() && bbs.blackKnights().popcount() < 3))
				return true;

			if (!bbs.nonPk().empty())
			{
				if (!bbs.whiteMinors().multiple() && !bbs.blackMinors().multiple())
					return true;

				// KBBKB
				if (bbs.nonPk() == bbs.bishops()
					&& (bbs.whiteBishops().popcount() < 3 && !bbs.blackBishops().multiple()
					|| bbs.blackBishops().popcount() < 3 && !bbs.whiteBishops().multiple()))
					return true;
			}

			return false;
		}

		class Evaluator
		{
		public:
			Evaluator(const Position &pos, PawnCache *pawnCache);

			[[nodiscard]] inline auto eval() const
			{
				return m_final;
			}

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
			};

			template <Color Us>
			auto evalPawnStructure(EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalPawns        (EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalKnights      (EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalBishops      (EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalRooks        (EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalQueens       (EvalData &ours, const EvalData &theirs) -> void;
			template <Color Us>
			auto evalKing         (EvalData &ours, const EvalData &theirs) -> void;

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
			const auto &bbs = pos.bbs();

			const auto blackPawns = bbs.blackPawns();
			const auto whitePawns = bbs.whitePawns();

			const auto blackPawnLeftAttacks = blackPawns.shiftDownLeft();
			const auto blackPawnRightAttacks = blackPawns.shiftDownRight();

			m_blackData.pawnAttacks = blackPawnLeftAttacks | blackPawnRightAttacks;

			const auto whitePawnLeftAttacks = whitePawns.shiftUpLeft();
			const auto whitePawnRightAttacks = whitePawns.shiftUpRight();

			m_whiteData.pawnAttacks = whitePawnLeftAttacks | whitePawnRightAttacks;

			m_blackData.semiOpen = ~blackPawns.fillFile();
			m_whiteData.semiOpen = ~whitePawns.fillFile();

			m_blackData.available = ~(bbs.blackOccupancy() | m_whiteData.pawnAttacks);
			m_whiteData.available = ~(bbs.whiteOccupancy() | m_blackData.pawnAttacks);

			m_openFiles = m_blackData.semiOpen & m_whiteData.semiOpen;

			m_total = pos.material().material;

			if (pawnCache)
			{
				auto &cacheEntry = pawnCache->probe(pos.pawnKey());

				if (cacheEntry.key == pos.pawnKey())
				{
					m_whiteData.pawnStructure = cacheEntry.eval;

					m_blackData.passers = cacheEntry.passers & bbs.blackOccupancy();
					m_whiteData.passers = cacheEntry.passers & bbs.whiteOccupancy();
				}
				else
				{
					evalPawnStructure<Color::Black>(m_blackData, m_whiteData);
					evalPawnStructure<Color::White>(m_whiteData, m_blackData);

					cacheEntry.key = pos.pawnKey();
					cacheEntry.eval = m_whiteData.pawnStructure - m_blackData.pawnStructure;
					cacheEntry.passers = m_blackData.passers | m_whiteData.passers;
				}
			}
			else
			{
				evalPawnStructure<Color::Black>(m_blackData, m_whiteData);
				evalPawnStructure<Color::White>(m_whiteData, m_blackData);
			}

			evalPawns<Color::Black>(m_blackData, m_whiteData);
			evalPawns<Color::White>(m_whiteData, m_blackData);

			evalKnights<Color::Black>(m_blackData, m_whiteData);
			evalKnights<Color::White>(m_whiteData, m_blackData);

			evalBishops<Color::Black>(m_blackData, m_whiteData);
			evalBishops<Color::White>(m_whiteData, m_blackData);

			evalRooks<Color::Black>(m_blackData, m_whiteData);
			evalRooks<Color::White>(m_whiteData, m_blackData);

			evalQueens<Color::Black>(m_blackData, m_whiteData);
			evalQueens<Color::White>(m_whiteData, m_blackData);

			evalKing<Color::Black>(m_blackData, m_whiteData);
			evalKing<Color::White>(m_whiteData, m_blackData);

			m_total += m_whiteData.pawnStructure - m_blackData.pawnStructure;

			m_total += m_whiteData.pawns         - m_blackData.pawns;
			m_total += m_whiteData.knights       - m_blackData.knights;
			m_total += m_whiteData.bishops       - m_blackData.bishops;
			m_total += m_whiteData.rooks         - m_blackData.rooks;
			m_total += m_whiteData.queens        - m_blackData.queens;
			m_total += m_whiteData.kings         - m_blackData.kings;

			m_total += m_whiteData.mobility      - m_blackData.mobility;

			m_final = pos.material().interp(m_total);

			if (isLikelyDrawn(pos))
				m_final /= 8;
		}

		template <Color Us>
		auto Evaluator::evalPawnStructure(EvalData &ours, const EvalData &theirs) -> void
		{
			constexpr auto Them = oppColor(Us);

			const auto &bbs = m_pos.bbs();

			const auto ourPawns = bbs.pawns<Us>();
			const auto theirPawns = bbs.pawns<Them>();

			const auto up = ourPawns.template shiftUpRelative<Us>();

			const auto doubledPawns = up & ourPawns;
			ours.pawnStructure += DoubledPawn * doubledPawns.popcount();

			ours.pawnStructure += PawnDefender * (ours.pawnAttacks & ourPawns).popcount();
			ours.pawnStructure += OpenPawn * (ourPawns
				& ~theirPawns.template fillDownRelative<Us>() & ~ours.pawnAttacks).popcount();

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
				else if (!(pawn & theirs.semiOpen).empty())
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
		auto Evaluator::evalPawns(EvalData &ours, const EvalData &theirs) -> void
		{
			constexpr auto Them = oppColor(Us);

			const auto &bbs = m_pos.bbs();

			ours.pawns += PawnAttackingMinor * (ours.pawnAttacks & bbs.template minors<Them>()).popcount();
			ours.pawns += PawnAttackingRook  * (ours.pawnAttacks & bbs.template  rooks<Them>()).popcount();
			ours.pawns += PawnAttackingQueen * (ours.pawnAttacks & bbs.template queens<Them>()).popcount();

			auto passers = ours.passers;
			while (!passers.empty())
			{
				const auto square = passers.popLowestSquare();
				const auto passer = Bitboard::fromSquare(square);

				const auto rank = relativeRank<Us>(squareRank(square));

				const auto promotion = toSquare(relativeRank<Us>(7), squareFile(square));

				if (bbs.template nonPk<Them>().empty()
					&& (std::min(5, chebyshev(square, promotion)) + (Us == m_pos.toMove() ? 1 : 0))
						< chebyshev(m_pos.template king<Them>(), promotion))
					ours.pawns += PasserSquareRule;

				if (!(passer.template shiftUpRelative<Us>() & bbs.occupancy()).empty())
					ours.pawns += BlockedPasser[rank];
			}
		}

		template <Color Us>
		auto Evaluator::evalKnights(EvalData &ours, const EvalData &theirs) -> void
		{
			constexpr auto Them = oppColor(Us);

			const auto &bbs = m_pos.bbs();

			auto knights = bbs.knights<Us>();

			if (knights.empty())
				return;

			ours.knights += MinorBehindPawn
				* (knights.template shiftUpRelative<Us>() & bbs.template pawns<Us>()).popcount();

			while (!knights.empty())
			{
				const auto square = knights.popLowestSquare();
				const auto knight = Bitboard::fromSquare(square);

				if ((AntiPasserMasks<Us>[static_cast<i32>(square)]
					& ~boards::Files[squareFile(square)]
					& bbs.template pawns<Them>()).empty() && !(knight & ours.pawnAttacks).empty())
					ours.knights += KnightOutpost;

				const auto attacks = attacks::getKnightAttacks(square);

				ours.knights += MinorAttackingRook  * (attacks & bbs.template  rooks<Them>()).popcount();
				ours.knights += MinorAttackingQueen * (attacks & bbs.template queens<Them>()).popcount();

				ours.mobility += KnightMobility[(attacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		auto Evaluator::evalBishops(EvalData &ours, const EvalData &theirs) -> void
		{
			constexpr auto Them = oppColor(Us);

			const auto &bbs = m_pos.bbs();

			auto bishops = bbs.bishops<Us>();

			if (bishops.empty())
				return;

			ours.bishops += MinorBehindPawn
				* (bishops.template shiftUpRelative<Us>() & bbs.template pawns<Us>()).popcount();

			if (!(bishops & boards::DarkSquares).empty()
				&& !(bishops & boards::LightSquares).empty())
				ours.bishops += BishopPair;

			const auto occupancy = bbs.occupancy();
			const auto xrayOcc = occupancy ^ bbs.template bishops<Us>() ^ bbs.template queens<Us>();

			while (!bishops.empty())
			{
				const auto square = bishops.popLowestSquare();

				const auto attacks = attacks::getBishopAttacks(square, occupancy);

				ours.bishops += MinorAttackingRook  * (attacks & bbs.template  rooks<Them>()).popcount();
				ours.bishops += MinorAttackingQueen * (attacks & bbs.template queens<Them>()).popcount();

				const auto mobilityAttacks = attacks::getBishopAttacks(square, xrayOcc);
				ours.mobility += BishopMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		auto Evaluator::evalRooks(EvalData &ours, const EvalData &theirs) -> void
		{
			constexpr auto Them = oppColor(Us);

			const auto &bbs = m_pos.bbs();

			auto rooks = bbs.rooks<Us>();

			if (rooks.empty())
				return;

			const auto occupancy = bbs.occupancy();
			const auto xrayOcc = occupancy ^ bbs.template rooks<Us>() ^ bbs.template queens<Us>();

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

				ours.rooks += RookAttackingQueen * (attacks & bbs.template queens<Them>()).popcount();

				const auto mobilityAttacks = attacks::getRookAttacks(square, xrayOcc);
				ours.mobility += RookMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		auto Evaluator::evalQueens(EvalData &ours, const EvalData &theirs) -> void
		{
			const auto &bbs = m_pos.bbs();

			auto queens = bbs.queens<Us>();

			if (queens.empty())
				return;

			const auto occupancy = bbs.occupancy();
			const auto xrayOcc = occupancy ^ bbs.template bishops<Us>()
			    ^ bbs.template rooks<Us>() ^ bbs.template queens<Us>();

			while (!queens.empty())
			{
				const auto square = queens.popLowestSquare();

				const auto mobilityAttacks = attacks::getQueenAttacks(square, xrayOcc);
				ours.mobility += QueenMobility[(mobilityAttacks & ours.available).popcount()];
			}
		}

		template <Color Us>
		auto Evaluator::evalKing(EvalData &ours, const EvalData &theirs) -> void
		{
			const auto &bbs = m_pos.bbs();

			const auto king = bbs.template kings<Us>();

			if (!(king & m_openFiles).empty())
				ours.kings += KingOnOpenFile;
			else if (!(king & ours.semiOpen).empty())
				ours.kings += KingOnSemiOpenFile;
		}
	}

	auto staticEval(const Position &pos, PawnCache *pawnCache, const Contempt &contempt) -> Score
	{
		auto eval = Evaluator{pos, pawnCache}.eval() * (pos.toMove() == Color::Black ? -1 : 1) + Tempo;
		eval += contempt[static_cast<i32>(pos.toMove())];
		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}
}
