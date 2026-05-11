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

#pragma once

#include "../types.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "../attacks/attacks.h"
#include "../keys.h"
#include "../move.h"
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
        void finalize(KingPair kings) {
            SP_UNUSED(kings);
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

        [[nodiscard]] u64 roughKeyAfter(Move move) const;

        [[nodiscard]] Bitboard allAttackersTo(Square sq, Bitboard occupancy) const;
        [[nodiscard]] Bitboard nonSliderAttackersTo(Square sq, Color attacker) const;
        [[nodiscard]] Bitboard attackersTo(Square sq, Color attacker) const;

        template <bool kThreatShortcut = true>
        [[nodiscard]] bool isAttacked(Color toMove, Square sq, Color attacker) const;

        template <bool kThreatShortcut = true>
        [[nodiscard]] inline bool isAttacked(Square sq, Color attacker) const {
            assert(sq != Squares::kNone);
            assert(attacker != Colors::kNone);
            return isAttacked<kThreatShortcut>(stm(), sq, attacker);
        }

        [[nodiscard]] bool anyAttacked(Bitboard squares, Color attacker) const;

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

        [[nodiscard]] inline std::array<Bitboard, 2> pinned() const {
            return m_pinned;
        }

        [[nodiscard]] inline Bitboard threats() const {
            return m_threats;
        }

        [[nodiscard]] bool hasUpcomingRepetition(i32 ply, std::span<const u64> keys) const;
        [[nodiscard]] bool isDrawn(i32 ply, std::span<const u64> keys) const;

        [[nodiscard]] Piece captureTarget(Move move) const;
        [[nodiscard]] bool isNoisy(Move move) const;
        [[nodiscard]] bool givesDirectCheck(Move move) const;

        [[nodiscard]] std::string toFen() const;

        [[nodiscard]] inline bool operator==(const Position& other) const = default;

        void regen();

        [[nodiscard]] Move moveFromUci(std::string_view move) const;

        [[nodiscard]] inline u32 plyFromStartpos() const {
            return m_fullmove * 2 - (m_stm == Colors::kBlack ? 0 : 1) - 1;
        }

        [[nodiscard]] inline i32 classicalMaterial() const {
            const auto& bbs = m_boards.bbs();

            return 1 * bbs.pawns().popcount()   //
                 + 3 * bbs.knights().popcount() //
                 + 3 * bbs.bishops().popcount() //
                 + 5 * bbs.rooks().popcount()   //
                 + 9 * bbs.queens().popcount();
        }

        [[nodiscard]] static Position startpos();

        [[nodiscard]] static std::optional<Position> fromFenParts(std::span<const std::string_view> fen);
        [[nodiscard]] static std::optional<Position> fromFen(std::string_view fen);

        [[nodiscard]] static std::optional<Position> fromFrcIndex(u32 n);
        [[nodiscard]] static std::optional<Position> fromDfrcIndex(u32 n);

    private:
        template <bool kUpdateKeys = true>
        void setPiece(Piece piece, Square sq);
        template <bool kUpdateKeys = true>
        void removePiece(Piece piece, Square sq);

        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        [[nodiscard]] Piece movePiece(Piece piece, Square src, Square dst, Observer observer);

        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        Piece promotePawn(Piece pawn, Square src, Square dst, PieceType promo, Observer observer);
        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        void castle(Piece king, Square kingSrc, Square rookSrc, Observer observer);
        template <bool kUpdateKeys = true, typename Observer = NullObserver>
        Piece enPassant(Piece pawn, Square src, Square dst, Observer observer);

        void calcCheckersAndPins();
        void calcThreats();
        void calcCheckZones();

        // Unsets ep squares if they are invalid (no pawn is able to capture)
        void filterEp(Color capturing);

        PositionBoards m_boards{};

        // pnbr
        std::array<Bitboard, 4> m_checkZones{};

        Keys m_keys{};

        Bitboard m_checkers{};
        std::array<Bitboard, 2> m_pinned{};
        Bitboard m_threats{};

        CastlingRooks m_castlingRooks{};

        u16 m_halfmove{};
        u32 m_fullmove{1};

        Square m_enPassant{Squares::kNone};

        KingPair m_kings{};

        Color m_stm{};
    };

    static_assert(sizeof(Position) == 248);
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Position> : fmt::formatter<std::string_view> {
    format_context::iterator format(const stormphrax::Position& value, format_context& ctx) const;
};
