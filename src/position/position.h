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

#pragma once

#include "../types.h"

#include <array>
#include <optional>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../attacks/attacks.h"
#include "../keys.h"
#include "../move.h"
#include "../rays.h"
#include "../ttable.h"
#include "boards.h"

namespace stormphrax {
    struct Keys {
        u64 all;
        u64 pawns;
        u64 blackNonPawns;
        u64 whiteNonPawns;
        u64 majors;

        inline void clear() {
            all = 0;
            pawns = 0;
            blackNonPawns = 0;
            whiteNonPawns = 0;
            majors = 0;
        }

        inline void flipStm() {
            all ^= keys::color();
        }

        inline void flipPiece(Piece piece, Square sq) {
            const auto key = keys::pieceSquare(piece, sq);

            all ^= key;

            if (piece.type() == PieceTypes::kPawn) {
                pawns ^= key;
            } else if (piece.color() == Colors::kBlack) {
                blackNonPawns ^= key;
            } else {
                whiteNonPawns ^= key;
            }

            if (piece.type().isMajor()) {
                majors ^= key;
            }
        }

        inline void movePiece(Piece piece, Square src, Square dst) {
            const auto key = keys::pieceSquare(piece, src) ^ keys::pieceSquare(piece, dst);

            all ^= key;

            if (piece.type() == PieceTypes::kPawn) {
                pawns ^= key;
            } else if (piece.color() == Colors::kBlack) {
                blackNonPawns ^= key;
            } else {
                whiteNonPawns ^= key;
            }

            if (piece.type().isMajor()) {
                majors ^= key;
            }
        }

        inline void flipEp(Square epSq) {
            const auto key = keys::enPassant(epSq);

            all ^= key;
            pawns ^= key;
        }

        inline void flipCastling(const CastlingRooks& rooks) {
            const auto key = keys::castling(rooks);

            all ^= key;
            blackNonPawns ^= key;
            whiteNonPawns ^= key;
            majors ^= key;
        }

        inline void switchCastling(const CastlingRooks& before, const CastlingRooks& after) {
            const auto key = keys::castling(before) ^ keys::castling(after);

            all ^= key;
            blackNonPawns ^= key;
            whiteNonPawns ^= key;
            majors ^= key;
        }

        [[nodiscard]] inline bool operator==(const Keys& other) const = default;
    };

    struct NullObserver {
        void prepareKingMove(Color color, Square src, Square dst) {
            SP_UNUSED(color, src, dst);
        }
        void pieceAdded(const PositionBoards& boards, Piece piece, Square sq) {
            SP_UNUSED(boards, piece, sq);
        }
        void pieceRemoved(const PositionBoards& boards, Piece piece, Square sq) {
            SP_UNUSED(boards, piece, sq);
        }
        void pieceMutated(const PositionBoards& boards, Piece oldPiece, Piece newPiece, Square sq) {
            SP_UNUSED(boards, oldPiece, newPiece, sq);
        }
        void pieceMoved(const PositionBoards& boards, Piece piece, Square src, Square dst) {
            SP_UNUSED(boards, piece, src, dst);
        }
        void piecePromoted(const PositionBoards& boards, Piece oldPiece, Square src, Piece newPiece, Square dst) {
            SP_UNUSED(boards, oldPiece, src, newPiece, dst);
        }
        void finalize(const PositionBoards& boards, KingPair kings) {
            SP_UNUSED(boards, kings);
        }
    };

    class Position {
    public:
        // Moves are assumed to be legal
        template <typename Observer>
        [[nodiscard]] Position applyMove(Move move, Observer observer) const;

        [[nodiscard]] Position applyMove(Move move) const {
            return applyMove(move, NullObserver{});
        }

        [[nodiscard]] inline Position applyNullMove() const {
            return applyMove(kNullMove);
        }

        [[nodiscard]] bool isPseudolegal(Move move) const;
        [[nodiscard]] bool isLegal(Move move) const;

        [[nodiscard]] inline const PositionBoards& boards() const {
            return m_boards;
        }

        [[nodiscard]] inline const BitboardSet& bbs() const {
            return m_boards.bbs();
        }

        [[nodiscard]] inline Color stm() const {
            return m_stm;
        }

        [[nodiscard]] inline Color nstm() const {
            return m_stm.flip();
        }

        [[nodiscard]] inline const CastlingRooks& castlingRooks() const {
            return m_castlingRooks;
        }

        [[nodiscard]] inline Square enPassant() const {
            return m_enPassant;
        }

        [[nodiscard]] inline u16 halfmove() const {
            return m_halfmove;
        }

        [[nodiscard]] inline u32 fullmove() const {
            return m_fullmove;
        }

        [[nodiscard]] inline u64 key() const {
            return m_keys.all;
        }

        [[nodiscard]] inline u64 pawnKey() const {
            return m_keys.pawns;
        }

        [[nodiscard]] inline u64 blackNonPawnKey() const {
            return m_keys.blackNonPawns;
        }

        [[nodiscard]] inline u64 whiteNonPawnKey() const {
            return m_keys.whiteNonPawns;
        }

        [[nodiscard]] inline u64 majorKey() const {
            return m_keys.majors;
        }

        [[nodiscard]] inline u64 roughKeyAfter(Move move) const {
            assert(move);

            const auto moving = m_boards.pieceOn(move.fromSq());
            assert(moving != Pieces::kNone);

            const auto captured = m_boards.pieceOn(move.toSq());

            auto key = m_keys.all;

            key ^= keys::pieceSquare(moving, move.fromSq());
            key ^= keys::pieceSquare(moving, move.toSq());

            if (captured != Pieces::kNone) {
                key ^= keys::pieceSquare(captured, move.toSq());
            }

            key ^= keys::color();

            return key;
        }

        [[nodiscard]] inline Bitboard allAttackersTo(Square sq, Bitboard occupancy) const {
            assert(sq != Squares::kNone);

            const auto& bbs = this->bbs();

            Bitboard attackers{};

            const auto queens = bbs.queens();

            const auto rooks = queens | bbs.rooks();
            attackers |= rooks & attacks::getRookAttacks(sq, occupancy);

            const auto bishops = queens | bbs.bishops();
            attackers |= bishops & attacks::getBishopAttacks(sq, occupancy);

            attackers |= bbs.blackPawns() & attacks::getPawnAttacks(sq, Colors::kWhite);
            attackers |= bbs.whitePawns() & attacks::getPawnAttacks(sq, Colors::kBlack);

            const auto knights = bbs.knights();
            attackers |= knights & attacks::getKnightAttacks(sq);

            const auto kings = bbs.kings();
            attackers |= kings & attacks::getKingAttacks(sq);

            return attackers;
        }

        [[nodiscard]] inline Bitboard attackersTo(Square sq, Color attacker) const {
            assert(sq != Squares::kNone);

            const auto& bbs = this->bbs();

            Bitboard attackers{};

            const auto occ = bbs.occupancy();

            const auto queens = bbs.queens(attacker);

            const auto rooks = queens | bbs.rooks(attacker);
            attackers |= rooks & attacks::getRookAttacks(sq, occ);

            const auto bishops = queens | bbs.bishops(attacker);
            attackers |= bishops & attacks::getBishopAttacks(sq, occ);

            const auto pawns = bbs.pawns(attacker);
            attackers |= pawns & attacks::getPawnAttacks(sq, attacker.flip());

            const auto knights = bbs.knights(attacker);
            attackers |= knights & attacks::getKnightAttacks(sq);

            const auto kings = bbs.kings(attacker);
            attackers |= kings & attacks::getKingAttacks(sq);

            return attackers;
        }

        template <bool kThreatShortcut = true>
        [[nodiscard]] inline bool isAttacked(Color toMove, Square sq, Color attacker) const {
            assert(toMove != Colors::kNone);
            assert(sq != Squares::kNone);
            assert(attacker != Colors::kNone);

            if constexpr (kThreatShortcut) {
                if (attacker != toMove) {
                    return m_threats[sq];
                }
            }

            const auto& bbs = m_boards.bbs();

            const auto occ = bbs.occupancy();

            if (const auto knights = bbs.knights(attacker); !(knights & attacks::getKnightAttacks(sq)).empty()) {
                return true;
            }

            if (const auto pawns = bbs.pawns(attacker); !(pawns & attacks::getPawnAttacks(sq, attacker.flip())).empty())
            {
                return true;
            }

            if (const auto kings = bbs.kings(attacker); !(kings & attacks::getKingAttacks(sq)).empty()) {
                return true;
            }

            const auto queens = bbs.queens(attacker);

            if (const auto bishops = queens | bbs.bishops(attacker);
                !(bishops & attacks::getBishopAttacks(sq, occ)).empty())
            {
                return true;
            }

            if (const auto rooks = queens | bbs.rooks(attacker); !(rooks & attacks::getRookAttacks(sq, occ)).empty()) {
                return true;
            }

            return false;
        }

        template <bool kThreatShortcut = true>
        [[nodiscard]] inline bool isAttacked(Square sq, Color attacker) const {
            assert(sq != Squares::kNone);
            assert(attacker != Colors::kNone);

            return isAttacked<kThreatShortcut>(stm(), sq, attacker);
        }

        [[nodiscard]] inline bool anyAttacked(Bitboard squares, Color attacker) const {
            assert(attacker != Colors::kNone);

            if (attacker == nstm()) {
                return !(squares & m_threats).empty();
            }

            while (squares) {
                const auto sq = squares.popLowestSquare();
                if (isAttacked(sq, attacker)) {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] inline KingPair kings() const {
            return m_kings;
        }

        [[nodiscard]] inline Square blackKing() const {
            return m_kings.black();
        }

        [[nodiscard]] inline Square whiteKing() const {
            return m_kings.white();
        }

        [[nodiscard]] inline Square king(Color c) const {
            assert(c != Colors::kNone);
            return m_kings.color(c);
        }

        [[nodiscard]] inline Square oppKing(Color c) const {
            assert(c != Colors::kNone);
            return m_kings.color(c.flip());
        }

        [[nodiscard]] inline bool isCheck() const {
            return !m_checkers.empty();
        }

        [[nodiscard]] inline Bitboard checkers() const {
            return m_checkers;
        }

        [[nodiscard]] inline Bitboard pinned(Color c) const {
            return m_pinned[c.idx()];
        }

        [[nodiscard]] inline Bitboard pinners(Color c) const {
            return m_pinners[c.idx()];
        }

        [[nodiscard]] inline Bitboard threats() const {
            return m_threats;
        }

        [[nodiscard]] bool hasCycle(i32 ply, std::span<const u64> keys) const;
        [[nodiscard]] bool isDrawn(i32 ply, std::span<const u64> keys) const;

        [[nodiscard]] inline Piece captureTarget(Move move) const {
            assert(move != kNullMove);

            const auto type = move.type();

            if (type == MoveType::kCastling) {
                return Pieces::kNone;
            } else if (type == MoveType::kEnPassant) {
                return boards().pieceOn(move.fromSq()).flipColor();
            } else {
                return boards().pieceOn(move.toSq());
            }
        }

        [[nodiscard]] inline bool isNoisy(Move move) const {
            assert(move != kNullMove);

            const auto type = move.type();

            return type != MoveType::kCastling
                && (type == MoveType::kEnPassant || move.promo() == PieceTypes::kQueen
                    || boards().pieceOn(move.toSq()) != Pieces::kNone);
        }

        [[nodiscard]] std::string toFen() const;

        [[nodiscard]] inline bool operator==(const Position& other) const = default;

        void regen();

        [[nodiscard]] Move moveFromUci(std::string_view move) const;

        [[nodiscard]] inline u32 plyFromStartpos() const {
            return m_fullmove * 2 - (m_stm == Colors::kBlack ? 0 : 1) - 1;
        }

        [[nodiscard]] inline i32 classicalMaterial() const {
            const auto& bbs = m_boards.bbs();

            return 1 * bbs.pawns().popcount() + 3 * bbs.knights().popcount() + 3 * bbs.bishops().popcount()
                 + 5 * bbs.rooks().popcount() + 9 * bbs.queens().popcount();
        }

        [[nodiscard]] static Position starting();

        [[nodiscard]] static std::optional<Position> fromFenParts(std::span<const std::string_view> fen);
        [[nodiscard]] static std::optional<Position> fromFen(std::string_view fen);

        [[nodiscard]] static std::optional<Position> fromFrcIndex(u32 n);
        [[nodiscard]] static std::optional<Position> fromDfrcIndex(u32 n);

    private:
        template <bool kUpdateKeys = true>
        void setPiece(Piece piece, Square sq);
        template <bool kUpdateKeys = true>
        void removePiece(Piece piece, Square sq);
        template <bool kUpdateKeys = true>
        void movePieceNoCap(Piece piece, Square src, Square dst);

        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        [[nodiscard]] Piece movePiece(Piece piece, Square src, Square dst, Observer observer);

        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        Piece promotePawn(Piece pawn, Square src, Square dst, PieceType promo, Observer observer);
        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        void castle(Piece king, Square kingSrc, Square rookSrc, Observer observer);
        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        Piece enPassant(Piece pawn, Square src, Square dst, Observer observer);

        inline void calcCheckers() {
            const auto color = stm();
            m_checkers = attackersTo(m_kings.color(color), color.flip());
        }

        inline void calcPinned() {
            m_pinned = {};
            m_pinners = {};

            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                auto& pinned = m_pinned[c.idx()];
                auto& pinners = m_pinners[c.flip().idx()];

                const auto king = m_kings.color(c);
                const auto opponent = c.flip();

                const auto& bbs = m_boards.bbs();

                const auto ourOcc = bbs.occupancy(c);
                const auto oppOcc = bbs.occupancy(opponent);

                const auto oppQueens = bbs.queens(opponent);

                auto potentialAttackers = attacks::getBishopAttacks(king, oppOcc) & (oppQueens | bbs.bishops(opponent))
                                        | attacks::getRookAttacks(king, oppOcc) & (oppQueens | bbs.rooks(opponent));

                while (potentialAttackers) {
                    const auto potentialAttacker = potentialAttackers.popLowestSquare();
                    const auto maybePinned = ourOcc & rayBetween(potentialAttacker, king);

                    if (maybePinned.one()) {
                        pinned |= maybePinned;
                        pinners[potentialAttacker] = true;
                    }
                }
            }
        }

        inline void calcThreats() {
            const auto us = stm();
            const auto them = us.flip();

            const auto& bbs = m_boards.bbs();

            m_threats = Bitboard{};

            const auto occ = bbs.occupancy();

            const auto queens = bbs.queens(them);

            auto rooks = queens | bbs.rooks(them);
            while (rooks) {
                const auto rook = rooks.popLowestSquare();
                m_threats |= attacks::getRookAttacks(rook, occ);
            }

            auto bishops = queens | bbs.bishops(them);
            while (bishops) {
                const auto bishop = bishops.popLowestSquare();
                m_threats |= attacks::getBishopAttacks(bishop, occ);
            }

            auto knights = bbs.knights(them);
            while (knights) {
                const auto knight = knights.popLowestSquare();
                m_threats |= attacks::getKnightAttacks(knight);
            }

            const auto pawns = bbs.pawns(them);
            if (them == Colors::kBlack) {
                m_threats |= pawns.shiftDownLeft() | pawns.shiftDownRight();
            } else {
                m_threats |= pawns.shiftUpLeft() | pawns.shiftUpRight();
            }

            m_threats |= attacks::getKingAttacks(m_kings.color(them));
        }

        // Unsets ep squares if they are invalid (no pawn is able to capture)
        void filterEp(Color capturing);

        PositionBoards m_boards{};

        Keys m_keys{};

        Bitboard m_checkers{};
        std::array<Bitboard, 2> m_pinned{};
        std::array<Bitboard, 2> m_pinners{};
        Bitboard m_threats{};

        CastlingRooks m_castlingRooks{};

        u16 m_halfmove{};
        u32 m_fullmove{1};

        Square m_enPassant{Squares::kNone};

        KingPair m_kings{};

        Color m_stm{};
    };

    static_assert(sizeof(Position) == 232);
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Position> : fmt::formatter<std::string_view> {
    format_context::iterator format(const stormphrax::Position& value, format_context& ctx) const;
};
