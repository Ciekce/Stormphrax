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
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "../attacks/attacks.h"
#include "../eval/nnue.h"
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

        inline void flipPiece(Piece piece, Square square) {
            const auto key = keys::pieceSquare(piece, square);

            all ^= key;

            if (pieceType(piece) == PieceType::Pawn) {
                pawns ^= key;
            } else if (pieceColor(piece) == Color::Black) {
                blackNonPawns ^= key;
            } else {
                whiteNonPawns ^= key;
            }

            if (isMajor(piece)) {
                majors ^= key;
            }
        }

        inline void movePiece(Piece piece, Square src, Square dst) {
            const auto key = keys::pieceSquare(piece, src) ^ keys::pieceSquare(piece, dst);

            all ^= key;

            if (pieceType(piece) == PieceType::Pawn) {
                pawns ^= key;
            } else if (pieceColor(piece) == Color::Black) {
                blackNonPawns ^= key;
            } else {
                whiteNonPawns ^= key;
            }

            if (isMajor(piece)) {
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

    [[nodiscard]] inline std::string squareToString(Square square) {
        constexpr auto Files = std::array{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
        constexpr auto Ranks = std::array{'1', '2', '3', '4', '5', '6', '7', '8'};

        const auto s = static_cast<u32>(square);
        return std::string{Files[s % 8], Ranks[s / 8]};
    }

    enum class NnueUpdateAction {
        None = 0,
        Queue,
        Apply,
    };

    class Position {
    public:
        // Moves are assumed to be legal
        template <NnueUpdateAction NnueAction = NnueUpdateAction::None>
        [[nodiscard]] Position applyMove(Move move, eval::NnueState* nnueState = nullptr) const;

        [[nodiscard]] inline Position applyNullMove() const {
            return applyMove(NullMove);
        }

        [[nodiscard]] bool isPseudolegal(Move move) const;
        [[nodiscard]] bool isLegal(Move move) const;

        [[nodiscard]] inline const PositionBoards& boards() const {
            return m_boards;
        }

        [[nodiscard]] inline const BitboardSet& bbs() const {
            return m_boards.bbs();
        }

        [[nodiscard]] inline Color toMove() const {
            return m_blackToMove ? Color::Black : Color::White;
        }

        [[nodiscard]] inline Color opponent() const {
            return m_blackToMove ? Color::White : Color::Black;
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

            const auto moving = m_boards.pieceAt(move.src());
            assert(moving != Piece::None);

            const auto captured = m_boards.pieceAt(move.dst());

            auto key = m_keys.all;

            key ^= keys::pieceSquare(moving, move.src());
            key ^= keys::pieceSquare(moving, move.dst());

            if (captured != Piece::None) {
                key ^= keys::pieceSquare(captured, move.dst());
            }

            key ^= keys::color();

            return key;
        }

        [[nodiscard]] inline Bitboard allAttackersTo(Square square, Bitboard occupancy) const {
            assert(square != Square::None);

            const auto& bbs = this->bbs();

            Bitboard attackers{};

            const auto queens = bbs.queens();

            const auto rooks = queens | bbs.rooks();
            attackers |= rooks & attacks::getRookAttacks(square, occupancy);

            const auto bishops = queens | bbs.bishops();
            attackers |= bishops & attacks::getBishopAttacks(square, occupancy);

            attackers |= bbs.blackPawns() & attacks::getPawnAttacks(square, Color::White);
            attackers |= bbs.whitePawns() & attacks::getPawnAttacks(square, Color::Black);

            const auto knights = bbs.knights();
            attackers |= knights & attacks::getKnightAttacks(square);

            const auto kings = bbs.kings();
            attackers |= kings & attacks::getKingAttacks(square);

            return attackers;
        }

        [[nodiscard]] inline Bitboard attackersTo(Square square, Color attacker) const {
            assert(square != Square::None);

            const auto& bbs = this->bbs();

            Bitboard attackers{};

            const auto occ = bbs.occupancy();

            const auto queens = bbs.queens(attacker);

            const auto rooks = queens | bbs.rooks(attacker);
            attackers |= rooks & attacks::getRookAttacks(square, occ);

            const auto bishops = queens | bbs.bishops(attacker);
            attackers |= bishops & attacks::getBishopAttacks(square, occ);

            const auto pawns = bbs.pawns(attacker);
            attackers |= pawns & attacks::getPawnAttacks(square, oppColor(attacker));

            const auto knights = bbs.knights(attacker);
            attackers |= knights & attacks::getKnightAttacks(square);

            const auto kings = bbs.kings(attacker);
            attackers |= kings & attacks::getKingAttacks(square);

            return attackers;
        }

        template <bool ThreatShortcut = true>
        [[nodiscard]] inline bool isAttacked(Color toMove, Square square, Color attacker) const {
            assert(toMove != Color::None);
            assert(square != Square::None);
            assert(attacker != Color::None);

            if constexpr (ThreatShortcut) {
                if (attacker != toMove) {
                    return m_threats[square];
                }
            }

            const auto& bbs = m_boards.bbs();

            const auto occ = bbs.occupancy();

            if (const auto knights = bbs.knights(attacker); !(knights & attacks::getKnightAttacks(square)).empty()) {
                return true;
            }

            if (const auto pawns = bbs.pawns(attacker);
                !(pawns & attacks::getPawnAttacks(square, oppColor(attacker))).empty())
            {
                return true;
            }

            if (const auto kings = bbs.kings(attacker); !(kings & attacks::getKingAttacks(square)).empty()) {
                return true;
            }

            const auto queens = bbs.queens(attacker);

            if (const auto bishops = queens | bbs.bishops(attacker);
                !(bishops & attacks::getBishopAttacks(square, occ)).empty())
            {
                return true;
            }

            if (const auto rooks = queens | bbs.rooks(attacker);
                !(rooks & attacks::getRookAttacks(square, occ)).empty())
            {
                return true;
            }

            return false;
        }

        template <bool ThreatShortcut = true>
        [[nodiscard]] inline bool isAttacked(Square square, Color attacker) const {
            assert(square != Square::None);
            assert(attacker != Color::None);

            return isAttacked<ThreatShortcut>(toMove(), square, attacker);
        }

        [[nodiscard]] inline bool anyAttacked(Bitboard squares, Color attacker) const {
            assert(attacker != Color::None);

            if (attacker == opponent()) {
                return !(squares & m_threats).empty();
            }

            while (squares) {
                const auto square = squares.popLowestSquare();
                if (isAttacked(square, attacker)) {
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

        template <Color C>
        [[nodiscard]] inline Square king() const {
            return m_kings.color(C);
        }

        [[nodiscard]] inline Square king(Color c) const {
            assert(c != Color::None);
            return m_kings.color(c);
        }

        template <Color C>
        [[nodiscard]] inline Square oppKing() const {
            return m_kings.color(oppColor(C));
        }

        [[nodiscard]] inline Square oppKing(Color c) const {
            assert(c != Color::None);
            return m_kings.color(oppColor(c));
        }

        [[nodiscard]] inline bool isCheck() const {
            return !m_checkers.empty();
        }

        [[nodiscard]] inline Bitboard checkers() const {
            return m_checkers;
        }
        [[nodiscard]] inline Bitboard pinned(Color c) const {
            return m_pinned[static_cast<i32>(c)];
        }

        [[nodiscard]] inline std::array<Bitboard, 2> pinned() const {
            return m_pinned;
        }

        [[nodiscard]] inline Bitboard threats() const {
            return m_threats;
        }

        [[nodiscard]] bool hasCycle(i32 ply, std::span<const u64> keys) const;
        [[nodiscard]] bool isDrawn(i32 ply, std::span<const u64> keys) const;

        [[nodiscard]] inline Piece captureTarget(Move move) const {
            assert(move != NullMove);

            const auto type = move.type();

            if (type == MoveType::Castling) {
                return Piece::None;
            } else if (type == MoveType::EnPassant) {
                return flipPieceColor(boards().pieceAt(move.src()));
            } else {
                return boards().pieceAt(move.dst());
            }
        }

        [[nodiscard]] inline bool isNoisy(Move move) const {
            assert(move != NullMove);

            const auto type = move.type();

            return type != MoveType::Castling
                && (type == MoveType::EnPassant || move.promo() == PieceType::Queen
                    || boards().pieceAt(move.dst()) != Piece::None);
        }

        [[nodiscard]] inline std::pair<bool, Piece> noisyCapturedPiece(Move move) const {
            assert(move != NullMove);

            const auto type = move.type();

            if (type == MoveType::Castling) {
                return {false, Piece::None};
            } else if (type == MoveType::EnPassant) {
                return {true, colorPiece(PieceType::Pawn, toMove())};
            } else {
                const auto captured = boards().pieceAt(move.dst());
                return {captured != Piece::None || move.promo() == PieceType::Queen, captured};
            }
        }

        [[nodiscard]] std::string toFen() const;

        [[nodiscard]] inline bool operator==(const Position& other) const = default;

        void regen();

        [[nodiscard]] Move moveFromUci(const std::string& move) const;

        [[nodiscard]] inline u32 plyFromStartpos() const {
            return m_fullmove * 2 - (m_blackToMove ? 0 : 1) - 1;
        }

        [[nodiscard]] inline i32 classicalMaterial() const {
            const auto& bbs = m_boards.bbs();

            return 1 * bbs.pawns().popcount() + 3 * bbs.knights().popcount() + 3 * bbs.bishops().popcount()
                 + 5 * bbs.rooks().popcount() + 9 * bbs.queens().popcount();
        }

        [[nodiscard]] static Position starting();
        [[nodiscard]] static std::optional<Position> fromFen(const std::string& fen);
        [[nodiscard]] static std::optional<Position> fromFrcIndex(u32 n);
        [[nodiscard]] static std::optional<Position> fromDfrcIndex(u32 n);

    private:
        template <bool UpdateKeys = true>
        void setPiece(Piece piece, Square square);
        template <bool UpdateKeys = true>
        void removePiece(Piece piece, Square square);
        template <bool UpdateKeys = true>
        void movePieceNoCap(Piece piece, Square src, Square dst);

        template <bool UpdateKeys = true, bool UpdateNnue = true>
        [[nodiscard]] Piece movePiece(Piece piece, Square src, Square dst, eval::NnueUpdates& nnueUpdates);

        template <bool UpdateKeys = true, bool UpdateNnue = true>
        Piece promotePawn(Piece pawn, Square src, Square dst, PieceType promo, eval::NnueUpdates& nnueUpdates);
        template <bool UpdateKeys = true, bool UpdateNnue = true>
        void castle(Piece king, Square kingSrc, Square rookSrc, eval::NnueUpdates& nnueUpdates);
        template <bool UpdateKeys = true, bool UpdateNnue = true>
        Piece enPassant(Piece pawn, Square src, Square dst, eval::NnueUpdates& nnueUpdates);

        [[nodiscard]] inline Bitboard calcCheckers() const {
            const auto color = toMove();
            return attackersTo(m_kings.color(color), oppColor(color));
        }

        [[nodiscard]] inline Bitboard calcPinned(Color c) const {
            Bitboard pinned{};

            const auto king = m_kings.color(c);
            const auto opponent = oppColor(c);

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
                }
            }

            return pinned;
        }

        [[nodiscard]] inline std::array<Bitboard, 2> calcPinned() const {
            return {calcPinned(Color::Black), calcPinned(Color::White)};
        }

        [[nodiscard]] inline Bitboard calcThreats() const {
            const auto us = toMove();
            const auto them = oppColor(us);

            const auto& bbs = m_boards.bbs();

            Bitboard threats{};

            const auto occ = bbs.occupancy();

            const auto queens = bbs.queens(them);

            auto rooks = queens | bbs.rooks(them);
            while (rooks) {
                const auto rook = rooks.popLowestSquare();
                threats |= attacks::getRookAttacks(rook, occ);
            }

            auto bishops = queens | bbs.bishops(them);
            while (bishops) {
                const auto bishop = bishops.popLowestSquare();
                threats |= attacks::getBishopAttacks(bishop, occ);
            }

            auto knights = bbs.knights(them);
            while (knights) {
                const auto knight = knights.popLowestSquare();
                threats |= attacks::getKnightAttacks(knight);
            }

            const auto pawns = bbs.pawns(them);
            if (them == Color::Black) {
                threats |= pawns.shiftDownLeft() | pawns.shiftDownRight();
            } else {
                threats |= pawns.shiftUpLeft() | pawns.shiftUpRight();
            }

            threats |= attacks::getKingAttacks(m_kings.color(them));

            return threats;
        }

        // Unsets ep squares if they are invalid (no pawn is able to capture)
        void filterEp(Color capturing);

        PositionBoards m_boards{};

        Keys m_keys{};

        Bitboard m_checkers{};
        std::array<Bitboard, 2> m_pinned{};
        Bitboard m_threats{};

        CastlingRooks m_castlingRooks{};

        u16 m_halfmove{};
        u32 m_fullmove{1};

        Square m_enPassant{Square::None};

        KingPair m_kings{};

        bool m_blackToMove{};
    };

    static_assert(sizeof(Position) == 216);

    [[nodiscard]] Square squareFromString(const std::string& str);
} // namespace stormphrax
