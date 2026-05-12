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

#include "types.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "attacks/attacks.h"
#include "keys.h"
#include "move.h"

namespace stormphrax {
    class BitboardSet {
    public:
        [[nodiscard]] inline Bitboard& bb(Color c) {
            return m_colors[c.idx()];
        }

        [[nodiscard]] inline Bitboard bb(Color c) const {
            return m_colors[c.idx()];
        }

        [[nodiscard]] inline Bitboard& bb(PieceType pt) {
            return m_pieces[pt.idx()];
        }

        [[nodiscard]] inline Bitboard bb(PieceType pt) const {
            return m_pieces[pt.idx()];
        }

        [[nodiscard]] inline Bitboard bb(PieceType pt, Color c) const {
            return m_pieces[pt.idx()] & bb(c);
        }

        [[nodiscard]] inline Bitboard bb(Piece piece) const {
            return bb(piece.type(), piece.color());
        }

        [[nodiscard]] inline Bitboard black() const {
            return m_colors[Colors::kBlack.idx()];
        }

        [[nodiscard]] inline Bitboard white() const {
            return m_colors[Colors::kWhite.idx()];
        }

        [[nodiscard]] inline Bitboard occ() const {
            return black() | white();
        }

        [[nodiscard]] inline Bitboard pawns() const {
            return bb(PieceTypes::kPawn);
        }

        [[nodiscard]] inline Bitboard knights() const {
            return bb(PieceTypes::kKnight);
        }

        [[nodiscard]] inline Bitboard bishops() const {
            return bb(PieceTypes::kBishop);
        }

        [[nodiscard]] inline Bitboard rooks() const {
            return bb(PieceTypes::kRook);
        }

        [[nodiscard]] inline Bitboard queens() const {
            return bb(PieceTypes::kQueen);
        }

        [[nodiscard]] inline Bitboard kings() const {
            return bb(PieceTypes::kKing);
        }

        [[nodiscard]] inline Bitboard blackPawns() const {
            return pawns() & black();
        }

        [[nodiscard]] inline Bitboard whitePawns() const {
            return pawns() & white();
        }

        [[nodiscard]] inline Bitboard blackKnights() const {
            return knights() & black();
        }

        [[nodiscard]] inline Bitboard whiteKnights() const {
            return knights() & white();
        }

        [[nodiscard]] inline Bitboard blackBishops() const {
            return bishops() & black();
        }

        [[nodiscard]] inline Bitboard whiteBishops() const {
            return bishops() & white();
        }

        [[nodiscard]] inline Bitboard blackRooks() const {
            return rooks() & black();
        }

        [[nodiscard]] inline Bitboard whiteRooks() const {
            return rooks() & white();
        }

        [[nodiscard]] inline Bitboard blackQueens() const {
            return queens() & black();
        }

        [[nodiscard]] inline Bitboard whiteQueens() const {
            return queens() & white();
        }

        [[nodiscard]] inline Bitboard blackKings() const {
            return kings() & black();
        }

        [[nodiscard]] inline Bitboard whiteKings() const {
            return kings() & white();
        }

        [[nodiscard]] inline Bitboard minors() const {
            return knights() | bishops();
        }

        [[nodiscard]] inline Bitboard blackMinors() const {
            return minors() & black();
        }

        [[nodiscard]] inline Bitboard whiteMinors() const {
            return minors() & white();
        }

        [[nodiscard]] inline Bitboard majors() const {
            return rooks() | queens();
        }

        [[nodiscard]] inline Bitboard blackMajors() const {
            return majors() & black();
        }

        [[nodiscard]] inline Bitboard whiteMajors() const {
            return majors() & white();
        }

        [[nodiscard]] inline Bitboard nonPk() const {
            return occ() ^ pawns() ^ kings();
        }

        [[nodiscard]] inline Bitboard blackNonPk() const {
            return black() ^ (pawns() | kings()) & black();
        }

        [[nodiscard]] inline Bitboard whiteNonPk() const {
            return white() ^ (pawns() | kings()) & white();
        }

        [[nodiscard]] inline Bitboard pawns(Color c) const {
            return bb(PieceTypes::kPawn, c);
        }

        [[nodiscard]] inline Bitboard knights(Color c) const {
            return bb(PieceTypes::kKnight, c);
        }

        [[nodiscard]] inline Bitboard bishops(Color c) const {
            return bb(PieceTypes::kBishop, c);
        }

        [[nodiscard]] inline Bitboard rooks(Color c) const {
            return bb(PieceTypes::kRook, c);
        }

        [[nodiscard]] inline Bitboard queens(Color c) const {
            return bb(PieceTypes::kQueen, c);
        }

        [[nodiscard]] inline Bitboard kings(Color c) const {
            return bb(PieceTypes::kKing, c);
        }

        [[nodiscard]] inline Bitboard minors(Color c) const {
            return c == Colors::kBlack ? blackMinors() : whiteMinors();
        }

        [[nodiscard]] inline Bitboard majors(Color c) const {
            return c == Colors::kBlack ? blackMajors() : whiteMajors();
        }

        [[nodiscard]] inline Bitboard nonPk(Color c) const {
            return c == Colors::kBlack ? blackNonPk() : whiteNonPk();
        }

        [[nodiscard]] inline bool operator==(const BitboardSet& other) const = default;

    private:
        std::array<Bitboard, 2> m_colors{};
        std::array<Bitboard, 6> m_pieces{};
    };

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

    class Position;

    struct NullObserver {
        void prepareKingMove(Color c, Square src, Square dst) {
            SP_UNUSED(c, src, dst);
        }

        void pieceAdded(const Position& pos, Piece piece, Square sq) {
            SP_UNUSED(pos, piece, sq);
        }

        void pieceRemoved(const Position& pos, Piece piece, Square sq) {
            SP_UNUSED(pos, piece, sq);
        }

        void pieceMutated(const Position& pos, Piece oldPiece, Piece newPiece, Square sq) {
            SP_UNUSED(pos, oldPiece, newPiece, sq);
        }

        void pieceMoved(const Position& pos, Piece piece, Square src, Square dst) {
            SP_UNUSED(pos, piece, src, dst);
        }

        void piecePromoted(const Position& pos, Piece oldPiece, Square src, Piece newPiece, Square dst) {
            SP_UNUSED(pos, oldPiece, src, newPiece, dst);
        }

        void finalize(const Position& pos) {
            (void)pos;
        }
    };

    class BoardIterator;

    class Position {
    public:
        Position() {
            m_mailbox.fill(Pieces::kNone);
        }

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

        [[nodiscard]] inline Piece pieceOn(Square sq) const {
            assert(sq != Squares::kNone);
            return m_mailbox[sq.idx()];
        }

        [[nodiscard]] inline Bitboard bb(Color c) const {
            return m_bbs.bb(c);
        }

        [[nodiscard]] inline Bitboard bb(PieceType pt) const {
            return m_bbs.bb(pt);
        }

        [[nodiscard]] inline Bitboard bb(PieceType pt, Color c) const {
            return m_bbs.bb(pt, c);
        }

        [[nodiscard]] inline Bitboard bb(Piece piece) const {
            return m_bbs.bb(piece);
        }

        [[nodiscard]] inline Bitboard occ() const {
            return m_bbs.occ();
        }

        [[nodiscard]] inline const BitboardSet& bbs() const {
            return m_bbs;
        }

        [[nodiscard]] inline std::span<const Piece, Squares::kCount> mailbox() const {
            return m_mailbox;
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

        [[nodiscard]] Bitboard allAttackersTo(Square sq, Bitboard occ) const;
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
            return m_fullmove * 2 - (m_stm == Colors::kWhite) - 1;
        }

        [[nodiscard]] inline i32 classicalMaterial() const {
            return 1 * m_bbs.pawns().popcount()   //
                 + 3 * m_bbs.knights().popcount() //
                 + 3 * m_bbs.bishops().popcount() //
                 + 5 * m_bbs.rooks().popcount()   //
                 + 9 * m_bbs.queens().popcount();
        }

        [[nodiscard]] inline BoardIterator begin() const;
        [[nodiscard]] inline BoardIterator end() const;

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

        void setPieceInternal(Square sq, Piece piece);
        void movePieceInternal(Square src, Square dst, Piece piece);
        void moveAndChangePieceInternal(Square src, Square dst, Piece moving, PieceType promo);
        void removePieceInternal(Square sq, Piece piece);

        void calcCheckersAndPins();
        void calcThreats();
        void calcCheckZones();

        // Unsets ep squares if they are invalid (no pawn is able to capture)
        void filterEp(Color capturing);

        [[nodiscard]] inline Piece& mailboxSlot(Square sq) {
            return m_mailbox[sq.idx()];
        }

        BitboardSet m_bbs{};
        std::array<Piece, Squares::kCount> m_mailbox{};

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

    class BoardIterator {
    public:
        constexpr BoardIterator& operator++() {
            m_bb.popLowestSquare();
            return *this;
        }

        [[nodiscard]] constexpr std::pair<Piece, Square> operator*() const {
            const auto sq = m_bb.lowestSquare();
            return {m_pos.pieceOn(sq), sq};
        }

        inline bool operator==(const BoardIterator& other) const {
            assert(&m_pos == &other.m_pos);
            return m_bb == other.m_bb;
        }

    private:
        const Position& m_pos;
        Bitboard m_bb;

        BoardIterator(const Position& pos, Bitboard bb) :
                m_pos{pos}, m_bb{bb} {}

        friend class Position;
    };

    inline BoardIterator Position::begin() const {
        return BoardIterator{*this, occ()};
    }

    inline BoardIterator Position::end() const {
        return BoardIterator{*this, Bitboard{}};
    }
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Position> : fmt::formatter<std::string_view> {
    format_context::iterator format(const stormphrax::Position& value, format_context& ctx) const;
};
