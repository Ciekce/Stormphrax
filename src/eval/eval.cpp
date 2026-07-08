/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include <algorithm>

#include "../tunable.h"

namespace stormphrax::eval {
    namespace {
#define S(Mg, Eg) \
    TaperedScore { \
        (Mg), (Eg) \
    }

        // pawn structure
        constexpr auto kDoubledPawn = S(-19, -21);
        constexpr auto kPawnDefender = S(18, 18);
        constexpr auto kOpenPawn = S(-12, -5);

        constexpr auto kPawnPhalanx =
            std::array{S(0, 0), S(3, 8), S(23, 12), S(27, 29), S(47, 66), S(126, 145), S(24, 275)};

        constexpr auto kPasser = std::array{S(0, 0), S(1, 12), S(-4, 19), S(-14, 52), S(13, 74), S(8, 150), S(51, 162)};

        constexpr auto kDefendedPasser =
            std::array{S(0, 0), S(0, 0), S(4, -9), S(5, -11), S(8, 0), S(35, 17), S(163, -12)};

        constexpr auto kBlockedPasser =
            std::array{S(0, 0), S(-10, -3), S(-9, 4), S(-5, -9), S(-14, -26), S(5, -93), S(30, -146)};

        constexpr auto kCandidatePasser =
            std::array{S(0, 0), S(8, -1), S(1, 2), S(3, 16), S(22, 19), S(49, 65), S(0, 0)};

        constexpr auto kDoubledPasser = S(18, -31);
        constexpr auto kPasserHelper = S(-8, 14);

        // pawns
        constexpr auto kPawnAttackingMinor = S(55, 18);
        constexpr auto kPawnAttackingRook = S(104, -33);
        constexpr auto kPawnAttackingQueen = S(60, -17);

        constexpr auto kPasserSquareRule = S(13, 108);

        // minors
        constexpr auto kMinorBehindPawn = S(6, 19);

        constexpr auto kMinorAttackingRook = S(42, 0);
        constexpr auto kMinorAttackingQueen = S(29, 2);

        // knights
        constexpr auto kKnightOutpost = S(27, 17);

        // bishops
        constexpr auto kBishopPair = S(28, 62);

        // rooks
        constexpr auto kRookOnOpenFile = S(43, 1);
        constexpr auto kRookOnSemiOpenFile = S(16, 8);
        constexpr auto kRookSupportingPasser = S(18, 15);
        constexpr auto kRookAttackingQueen = S(59, -24);

        // queens

        // kings
        constexpr auto kKingOnOpenFile = S(-75, 1);
        constexpr auto kKingOnSemiOpenFile = S(-32, 18);

        // mobility
        constexpr auto kKnightMobility = std::array{
            S(-44, -13),
            S(-24, -8),
            S(-13, -5),
            S(-8, 0),
            S(3, 3),
            S(9, 12),
            S(17, 10),
            S(22, 9),
            S(38, -9)
        };

        constexpr auto kBishopMobility = std::array{
            S(-56, 4),
            S(-41, -14),
            S(-27, -24),
            S(-19, -17),
            S(-10, -8),
            S(-5, 1),
            S(0, 7),
            S(3, 9),
            S(2, 14),
            S(12, 10),
            S(23, 4),
            S(49, 0),
            S(7, 25),
            S(62, -11)
        };

        constexpr auto kRookMobility = std::array{
            S(-44, -40),
            S(-31, -16),
            S(-24, -16),
            S(-19, -11),
            S(-18, -7),
            S(-12, -4),
            S(-9, 2),
            S(-4, 5),
            S(5, 7),
            S(12, 10),
            S(15, 13),
            S(25, 15),
            S(26, 19),
            S(45, 11),
            S(36, 12)
        };

        constexpr auto kQueenMobility =
            std::array{S(-33, 67),  S(-33, 235), S(-34, 95), S(-35, 56), S(-33, 52), S(-26, -24), S(-21, -62),
                       S(-18, -72), S(-15, -70), S(-9, -78), S(-7, -63), S(-4, -52), S(-5, -48),  S(4, -42),
                       S(5, -31),   S(1, -15),   S(0, -4),   S(17, -19), S(12, -5),  S(29, -9),   S(35, -5),
                       S(68, -20),  S(47, -4),   S(88, -13), S(37, 4),   S(44, 1),   S(-45, 66),  S(-70, 61)};
#undef S

        [[nodiscard]] consteval std::array<Bitboard, 64> generateAntiPasserMasks(Color us) {
            std::array<Bitboard, 64> dst{};

            for (u32 i = 0; i < 64; ++i) {
                dst[i] = Bitboard::fromSquare(Square::fromRaw(i));
                dst[i] |= dst[i].shiftLeft() | dst[i].shiftRight();

                dst[i] = dst[i].shiftUpRelative(us).fillUpRelative(us);
            }

            return dst;
        }

        constexpr auto kAntiPasserMasks = std::array{
            generateAntiPasserMasks(Colors::kBlack),
            generateAntiPasserMasks(Colors::kWhite),
        };

        [[nodiscard]] consteval std::array<Bitboard, 64> generatePawnHelperMasks(Color us) {
            std::array<Bitboard, 64> dst{};

            for (u32 i = 0; i < 64; ++i) {
                dst[i] = Bitboard::fromSquare(Square::fromRaw(i));
                dst[i] = dst[i].shiftLeft() | dst[i].shiftRight();

                dst[i] = dst[i].shiftDownRelative(us).fillDownRelative(us);
            }

            return dst;
        }

        constexpr auto kPawnHelperMasks = std::array{
            generatePawnHelperMasks(Colors::kBlack),
            generatePawnHelperMasks(Colors::kWhite),
        };

        [[nodiscard]] constexpr i32 chebyshev(Square s1, Square s2) {
            return std::max(util::abs(s2.file() - s1.file()), util::abs(s2.rank() - s1.rank()));
        }

        [[nodiscard]] inline bool isLikelyDrawn(const Position& pos) {
            const auto& bbs = pos.bbs();

            if (!bbs.pawns().empty() || !bbs.majors().empty()) {
                return false;
            }

            // KNK or KNNK
            if ((bbs.blackNonPk().empty() && bbs.whiteNonPk() == bbs.whiteKnights()
                 && bbs.whiteKnights().popcount() < 3)
                || (bbs.whiteNonPk().empty() && bbs.blackNonPk() == bbs.blackKnights()
                    && bbs.blackKnights().popcount() < 3))
            {
                return true;
            }

            if (!bbs.nonPk().empty()) {
                if (!bbs.whiteMinors().multiple() && !bbs.blackMinors().multiple()) {
                    return true;
                }

                // KBBKB
                if (bbs.nonPk() == bbs.bishops()
                    && ((bbs.whiteBishops().popcount() < 3 && !bbs.blackBishops().multiple())
                        || (bbs.blackBishops().popcount() < 3 && !bbs.whiteBishops().multiple())))
                {
                    return true;
                }
            }

            return false;
        }

        class Evaluator {
        public:
            Evaluator(const Position& pos, PawnCache* pawnCache);

            [[nodiscard]] inline auto eval() const {
                return m_final;
            }

        private:
            struct EvalData {
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

            void evalPawnStructure(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalPawns(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalKnights(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalBishops(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalRooks(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalQueens(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);
            void evalKing(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs);

            const Position& m_pos;

            EvalData m_blackData{};
            EvalData m_whiteData{};

            Bitboard m_openFiles{};

            TaperedScore m_total{};
            Score m_final;
        };

        Evaluator::Evaluator(const Position& pos, PawnCache* pawnCache) :
                m_pos{pos} {
            const auto& bbs = pos.bbs();

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

            m_blackData.available = ~(bbs.black() | m_whiteData.pawnAttacks);
            m_whiteData.available = ~(bbs.white() | m_blackData.pawnAttacks);

            m_openFiles = m_blackData.semiOpen & m_whiteData.semiOpen;

            m_total = pos.material().material;

            if (pawnCache) {
                auto& cacheEntry = pawnCache->probe(pos.pawnKey());

                if (cacheEntry.key == pos.pawnKey()) {
                    m_whiteData.pawnStructure = cacheEntry.eval;

                    m_blackData.passers = cacheEntry.passers & bbs.black();
                    m_whiteData.passers = cacheEntry.passers & bbs.white();
                } else {
                    evalPawnStructure(Colors::kBlack, m_blackData, m_whiteData);
                    evalPawnStructure(Colors::kWhite, m_whiteData, m_blackData);

                    cacheEntry.key = pos.pawnKey();
                    cacheEntry.eval = m_whiteData.pawnStructure - m_blackData.pawnStructure;
                    cacheEntry.passers = m_blackData.passers | m_whiteData.passers;
                }
            } else {
                evalPawnStructure(Colors::kBlack, m_blackData, m_whiteData);
                evalPawnStructure(Colors::kWhite, m_whiteData, m_blackData);
            }

            evalPawns(Colors::kBlack, m_blackData, m_whiteData);
            evalPawns(Colors::kWhite, m_whiteData, m_blackData);

            evalKnights(Colors::kBlack, m_blackData, m_whiteData);
            evalKnights(Colors::kWhite, m_whiteData, m_blackData);

            evalBishops(Colors::kBlack, m_blackData, m_whiteData);
            evalBishops(Colors::kWhite, m_whiteData, m_blackData);

            evalRooks(Colors::kBlack, m_blackData, m_whiteData);
            evalRooks(Colors::kWhite, m_whiteData, m_blackData);

            evalQueens(Colors::kBlack, m_blackData, m_whiteData);
            evalQueens(Colors::kWhite, m_whiteData, m_blackData);

            evalKing(Colors::kBlack, m_blackData, m_whiteData);
            evalKing(Colors::kWhite, m_whiteData, m_blackData);

            m_total += m_whiteData.pawnStructure - m_blackData.pawnStructure;

            m_total += m_whiteData.pawns - m_blackData.pawns;
            m_total += m_whiteData.knights - m_blackData.knights;
            m_total += m_whiteData.bishops - m_blackData.bishops;
            m_total += m_whiteData.rooks - m_blackData.rooks;
            m_total += m_whiteData.queens - m_blackData.queens;
            m_total += m_whiteData.kings - m_blackData.kings;

            m_total += m_whiteData.mobility - m_blackData.mobility;

            m_final = pos.material().interp(m_total);

            if (isLikelyDrawn(pos)) {
                m_final /= 8;
            }
        }

        void Evaluator::evalPawnStructure(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto& bbs = m_pos.bbs();

            const auto ourPawns = bbs.pawns(us);
            const auto theirPawns = bbs.pawns(us.flip());

            const auto up = ourPawns.shiftUpRelative(us);

            const auto doubledPawns = up & ourPawns;
            ours.pawnStructure += kDoubledPawn * doubledPawns.popcount();

            ours.pawnStructure += kPawnDefender * (ours.pawnAttacks & ourPawns).popcount();
            ours.pawnStructure +=
                kOpenPawn * (ourPawns & ~theirPawns.fillDownRelative(us) & ~ours.pawnAttacks).popcount();

            auto phalanx = ourPawns & ourPawns.shiftLeft();
            while (!phalanx.empty()) {
                const auto square = phalanx.popLowestSquare();
                const auto rank = relativeRank(us, square.rank());

                ours.pawnStructure += kPawnPhalanx[rank];
            }

            auto pawns = ourPawns;
            while (!pawns.empty()) {
                const auto square = pawns.popLowestSquare();
                const auto pawn = Bitboard::fromSquare(square);

                const auto rank = relativeRank(us, square.rank());

                const auto antiPassers = theirPawns & kAntiPasserMasks[us.idx()][square.idx()];

                if (antiPassers.empty()) {
                    ours.pawnStructure += kPasser[rank];

                    if (!(pawn & ours.pawnAttacks).empty()) {
                        ours.pawnStructure += kDefendedPasser[rank];
                    }

                    if (!(pawn & doubledPawns).empty()) {
                        ours.pawnStructure += kDoubledPasser;
                    }

                    const auto helpers = ourPawns & kPawnHelperMasks[us.idx()][square.idx()];
                    ours.pawnStructure += kPasserHelper * helpers.popcount();

                    ours.passers |= pawn;
                } else if (!(pawn & theirs.semiOpen).empty()) {
                    const auto stop = pawn.shiftUpRelative(us);

                    const auto levers = antiPassers & (pawn.shiftUpLeftRelative(us) | pawn.shiftUpRightRelative(us));

                    if (antiPassers == levers) {
                        ours.pawnStructure += kCandidatePasser[rank];
                    } else {
                        const auto telelevers =
                            antiPassers & (stop.shiftUpLeftRelative(us) | stop.shiftUpRightRelative(us));
                        const auto helpers = ourPawns & (pawn.shiftLeft() | pawn.shiftRight());

                        if (antiPassers == telelevers || telelevers.popcount() <= helpers.popcount()) {
                            ours.pawnStructure += kCandidatePasser[rank];
                        }
                    }
                }
            }
        }

        void Evaluator::evalPawns(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto them = us.flip();

            const auto& bbs = m_pos.bbs();

            ours.pawns += kPawnAttackingMinor * (ours.pawnAttacks & bbs.minors(them)).popcount();
            ours.pawns += kPawnAttackingRook * (ours.pawnAttacks & bbs.rooks(them)).popcount();
            ours.pawns += kPawnAttackingQueen * (ours.pawnAttacks & bbs.queens(them)).popcount();

            auto passers = ours.passers;
            while (!passers.empty()) {
                const auto square = passers.popLowestSquare();
                const auto passer = Bitboard::fromSquare(square);

                const auto rank = relativeRank(us, square.rank());

                const auto promotion = Square::fromFileRank(square.file(), relativeRank(us, 7));

                if (bbs.nonPk(them).empty()
                    && (std::min(5, chebyshev(square, promotion)) + (us == m_pos.stm()))
                           < chebyshev(m_pos.king(them), promotion))
                {
                    ours.pawns += kPasserSquareRule;
                }

                if (!(passer.shiftUpRelative(us) & bbs.occ()).empty()) {
                    ours.pawns += kBlockedPasser[rank];
                }
            }
        }

        void Evaluator::evalKnights(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto them = us.flip();

            const auto& bbs = m_pos.bbs();

            auto knights = bbs.knights(us);

            if (knights.empty()) {
                return;
            }

            ours.knights += kMinorBehindPawn * (knights.shiftUpRelative(us) & bbs.pawns(us)).popcount();

            while (!knights.empty()) {
                const auto square = knights.popLowestSquare();
                const auto knight = Bitboard::fromSquare(square);

                if ((kAntiPasserMasks[us.idx()][square.idx()] & ~boards::kFiles[square.file()] & bbs.pawns(them))
                        .empty()
                    && !(knight & ours.pawnAttacks).empty())
                {
                    ours.knights += kKnightOutpost;
                }

                const auto attacks = attacks::getKnightAttacks(square);

                ours.knights += kMinorAttackingRook * (attacks & bbs.rooks(them)).popcount();
                ours.knights += kMinorAttackingQueen * (attacks & bbs.queens(them)).popcount();

                ours.mobility += kKnightMobility[(attacks & ours.available).popcount()];
            }
        }

        void Evaluator::evalBishops(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto them = us.flip();

            const auto& bbs = m_pos.bbs();

            auto bishops = bbs.bishops(us);

            if (bishops.empty()) {
                return;
            }

            ours.bishops += kMinorBehindPawn * (bishops.shiftUpRelative(us) & bbs.pawns(us)).popcount();

            if (!(bishops & boards::kDarkSquares).empty() && !(bishops & boards::kLightSquares).empty()) {
                ours.bishops += kBishopPair;
            }

            const auto occupancy = bbs.occ();
            const auto xrayOcc = occupancy ^ bbs.bishops(us) ^ bbs.queens(us);

            while (!bishops.empty()) {
                const auto square = bishops.popLowestSquare();

                const auto attacks = attacks::getBishopAttacks(square, occupancy);

                ours.bishops += kMinorAttackingRook * (attacks & bbs.rooks(them)).popcount();
                ours.bishops += kMinorAttackingQueen * (attacks & bbs.queens(them)).popcount();

                const auto mobilityAttacks = attacks::getBishopAttacks(square, xrayOcc);
                ours.mobility += kBishopMobility[(mobilityAttacks & ours.available).popcount()];
            }
        }

        void Evaluator::evalRooks(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto them = us.flip();

            const auto& bbs = m_pos.bbs();

            auto rooks = bbs.rooks(us);

            if (rooks.empty()) {
                return;
            }

            const auto occupancy = bbs.occ();
            const auto xrayOcc = occupancy ^ bbs.rooks(us) ^ bbs.queens(us);

            while (!rooks.empty()) {
                const auto square = rooks.lowestSquare();
                const auto rook = rooks.popLowestBit();

                if (!(rook & m_openFiles).empty()) {
                    ours.rooks += kRookOnOpenFile;
                } else if (!(rook & ours.semiOpen).empty()) {
                    ours.rooks += kRookOnSemiOpenFile;
                }

                if (!(rook.fillUpRelative(us) & ours.passers).empty()) {
                    ours.rooks += kRookSupportingPasser;
                }

                const auto attacks = attacks::getRookAttacks(square, occupancy);

                ours.rooks += kRookAttackingQueen * (attacks & bbs.queens(them)).popcount();

                const auto mobilityAttacks = attacks::getRookAttacks(square, xrayOcc);
                ours.mobility += kRookMobility[(mobilityAttacks & ours.available).popcount()];
            }
        }

        void Evaluator::evalQueens(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto& bbs = m_pos.bbs();

            auto queens = bbs.queens(us);

            if (queens.empty()) {
                return;
            }

            const auto occupancy = bbs.occ();
            const auto xrayOcc = occupancy ^ bbs.bishops(us) ^ bbs.rooks(us) ^ bbs.queens(us);

            while (!queens.empty()) {
                const auto square = queens.popLowestSquare();

                const auto mobilityAttacks = attacks::getQueenAttacks(square, xrayOcc);
                ours.mobility += kQueenMobility[(mobilityAttacks & ours.available).popcount()];
            }
        }

        void Evaluator::evalKing(Color us, EvalData& ours, [[maybe_unused]] const EvalData& theirs) {
            const auto& bbs = m_pos.bbs();

            const auto king = bbs.kings(us);

            if (!(king & m_openFiles).empty()) {
                ours.kings += kKingOnOpenFile;
            } else if (!(king & ours.semiOpen).empty()) {
                ours.kings += kKingOnSemiOpenFile;
            }
        }

        [[nodiscard]] Score adjustStatic(const Position& pos, const Contempt& contempt, Score eval) {
            eval += contempt[pos.stm().idx()];
            return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
        }
    } // namespace

    template <bool kCorrect>
    [[nodiscard]] Score adjustEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* corrhist,
        i32 eval,
        i32* corrDelta
    ) {
        using namespace tunable;

        const auto bbs = pos.bbs();

        const auto npMaterial = scalingValuePawn() * bbs.pawns().popcount()     //
                              + scalingValueKnight() * bbs.knights().popcount() //
                              + scalingValueBishop() * bbs.bishops().popcount() //
                              + scalingValueRook() * bbs.rooks().popcount()     //
                              + scalingValueQueen() * bbs.queens().popcount();

        eval = (eval * (materialScalingBase() + npMaterial)
                + optimism[pos.stm().idx()] * (optimismBase() + npMaterial * optimismMaterialScale() / 1024))
             / 32768;

        eval = eval * (200 - pos.halfmove()) / 200;

        if constexpr (kCorrect) {
            const auto correction = corrhist->correction(pos, keyHistory);

            if (corrDelta) {
                *corrDelta = std::abs(correction);
            }

            eval += correction / 2048;
        }

        return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
    }

    template Score adjustEval<
        false>(const Position&, const Optimism&, std::span<const u64>, const CorrectionHistoryTable*, i32, i32*);
    template Score adjustEval<
        true>(const Position&, const Optimism&, std::span<const u64>, const CorrectionHistoryTable*, i32, i32*);

    Score staticEval(const Position& pos, PawnCache* pawnCache, const Contempt& contempt) {
        const auto evaluator = Evaluator{pos, pawnCache};
        const auto eval = evaluator.eval() * (pos.stm() == Colors::kBlack ? -1 : 1) + kTempo;
        return adjustStatic(pos, contempt, eval);
    }

    template <bool kCorrect>
    Score adjustedStaticEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* corrhist,
        PawnCache* pawnCache,
        const Contempt& contempt
    ) {
        const auto eval = staticEval(pos, pawnCache, contempt);
        return adjustEval<kCorrect>(pos, optimism, keyHistory, corrhist, eval);
    }

    template Score adjustedStaticEval<false>(
        const Position&,
        const Optimism&,
        std::span<const u64>,
        const CorrectionHistoryTable*,
        PawnCache*,
        const Contempt&
    );
    template Score adjustedStaticEval<true>(
        const Position&,
        const Optimism&,
        std::span<const u64>,
        const CorrectionHistoryTable*,
        PawnCache*,
        const Contempt&
    );

    Score staticEvalOnce(const Position& pos, const Contempt& contempt) {
        return staticEval(pos, nullptr, contempt);
    }
} // namespace stormphrax::eval
