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

#include "nnue.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <string_view>

#include "../attacks/attacks.h"
#include "../rays.h"
#include "../util/bits.h"
#include "../util/memstream.h"
#include "nnue/features/threats/geometry.h"
#include "nnue/io_impl.h"

#ifdef _MSC_VER
    #define SP_MSVC
    #pragma push_macro("_MSC_VER")
    #undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "../3rdparty/incbin.h"

#ifdef SP_MSVC
    #pragma pop_macro("_MSC_VER")
    #undef SP_MSVC
#endif

namespace {
    INCBIN(std::byte, defaultNet, SP_NETWORK_FILE);
} // namespace

namespace stormphrax::eval {
    namespace {
        SP_ENUM_FLAGS(u16, NetworkFlags){
            kNone = 0x0000,
            kZstdCompressed = 0x0001,
            kHorizontallyMirrored = 0x0002,
            kMergedKings = 0x0004,
            kPairwiseMul = 0x0008,
        };

        constexpr u16 kExpectedHeaderVersion = 1;

        struct __attribute__((packed)) NetworkHeader {
            std::array<char, 4> magic{};
            u16 version{};
            NetworkFlags flags{};
            [[maybe_unused]] u8 padding{};
            u8 arch{};
            u8 activation{};
            u16 hiddenSize{};
            u8 inputBuckets{};
            u8 outputBuckets{};
            u8 nameLen{};
            std::array<char, 48> name{};
        };

        static_assert(sizeof(NetworkHeader) == 64);

        inline std::string_view archName(u8 arch) {
            static constexpr std::array kNetworkArchNames = {
                "basic",
                "perspective",
                "perspective_multilayer",
                "perspective_multilayer_dual_act",
            };

            if (arch < kNetworkArchNames.size()) {
                return kNetworkArchNames[arch];
            }

            return "<unknown>";
        }

        inline std::string_view activationFuncName(u8 func) {
            static constexpr std::array kActivationFunctionNames = {"crelu", "screlu", "relu"};

            if (func < kActivationFunctionNames.size()) {
                return kActivationFunctionNames[func];
            }

            return "<unknown>";
        }

        //TODO better error messages
        bool validate(const NetworkHeader& header) {
            if (header.magic != std::array{'C', 'B', 'N', 'F'}) {
                println("invalid magic bytes in network header");
                return false;
            }

            if (header.version != kExpectedHeaderVersion) {
                eprintln(
                    "unsupported network format version {} (expected: {})",
                    header.version,
                    kExpectedHeaderVersion
                );
                return false;
            }

            if (header.arch != LayeredArch::kArchId) {
                eprintln(
                    "wrong network architecture {} (expected: {})",
                    archName(header.arch),
                    archName(LayeredArch::kArchId)
                );
                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kHorizontallyMirrored) != InputFeatureSet::kIsMirrored) {
                if constexpr (InputFeatureSet::kIsMirrored) {
                    eprintln("unmirrored network, expected horizontally mirrored");
                } else {
                    eprintln("horizontally mirrored network, expected unmirrored");
                }

                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kMergedKings) != InputFeatureSet::kMergedKings) {
                if constexpr (InputFeatureSet::kMergedKings) {
                    eprintln("network does not have merged king planes, expected merged");
                } else {
                    eprintln("network has merged king planes, expected unmerged");
                }

                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kPairwiseMul) != LayeredArch::kPairwise) {
                if constexpr (LayeredArch::kPairwise) {
                    eprintln("network L1 does not require pairwise multiplication, expected paired");
                } else {
                    eprintln("network L1 requires pairwise multiplication, expected unpaired");
                }

                return false;
            }

            if (header.activation != L1Activation::kId) {
                eprintln(
                    "wrong l1 activation function {} (expected: {})",
                    activationFuncName(header.activation),
                    activationFuncName(L1Activation::kId)
                );
                return false;
            }

            if (header.hiddenSize != kL1Size) {
                eprintln("wrong number of l1 neurons {} (expected: {})", header.hiddenSize, kL1Size);
                return false;
            }

            const bool headerThreatInputs = (header.inputBuckets & 0b10000000) != 0;
            const auto headerInputBuckets = header.inputBuckets & ~0b10000000;

            if (headerThreatInputs != InputFeatureSet::kThreatInputs) {
                if constexpr (InputFeatureSet::kThreatInputs) {
                    eprintln("network does not have the expected threat inputs");
                } else {
                    eprintln("network unexpectedly has threat inputs");
                }

                return false;
            }

            if (headerInputBuckets != InputFeatureSet::kBucketCount) {
                eprintln(
                    "wrong number of input buckets {} (expected: {})",
                    header.inputBuckets,
                    InputFeatureSet::kBucketCount
                );
                return false;
            }

            if (header.outputBuckets != OutputBucketing::kBucketCount) {
                eprintln(
                    "wrong number of output buckets {} (expected: {})",
                    header.outputBuckets,
                    OutputBucketing::kBucketCount
                );
                return false;
            }

            return true;
        }

        bool loadNetworkFrom(Network& network, std::istream& stream, const NetworkHeader& header) {
            bool success;

            if (testFlags(header.flags, NetworkFlags::kZstdCompressed)) {
                nnue::ZstdParamStream paramStream{stream};
                success = network.readFrom(paramStream);
            } else {
                nnue::PaddedParamStream<64> paramStream{stream};
                success = network.readFrom(paramStream);
            }

            return success;
        }

        Network s_network{};
    } // namespace

    const Network& g_network = s_network;

    void loadDefaultNetwork() {
        if (g_defaultNetSize < sizeof(NetworkHeader)) {
            eprintln("Missing default network?");
            return;
        }

        const auto& header = *reinterpret_cast<const NetworkHeader*>(g_defaultNetData);

        if (!validate(header)) {
            eprintln("Failed to validate default network header");
            return;
        }

        const auto* begin = g_defaultNetData + sizeof(NetworkHeader);
        const auto* end = g_defaultNetData + g_defaultNetSize;

        util::MemoryIstream stream{{begin, end}};

        if (!loadNetworkFrom(s_network, stream, header)) {
            eprintln("Failed to load default network");
            return;
        }
    }

    void loadNetwork(std::string_view name) {
        std::ifstream stream{std::string{name}, std::ios::binary};

        if (!stream) {
            eprintln("failed to open network file \"{}\"", name);
            return;
        }

        NetworkHeader header{};
        stream.read(reinterpret_cast<char*>(&header), sizeof(NetworkHeader));

        if (!stream) {
            eprintln("failed to read network file header");
            return;
        }

        if (!validate(header)) {
            return;
        }

        if (!loadNetworkFrom(s_network, stream, header)) {
            eprintln("failed to read network parameters");
            return;
        }

        const std::string_view netName{header.name.data(), header.nameLen};
        println("info string loaded network {}", netName);
    }

    std::string_view defaultNetworkName() {
        const auto& header = *reinterpret_cast<const NetworkHeader*>(g_defaultNetData);
        return {header.name.data(), header.nameLen};
    }

    void addThreatFeatures(std::span<i16, kL1Size> acc, Color c, const PositionBoards& boards, Square king) {
        const auto activateFeature = [&](u32 feature) {
            const auto* start = &g_network.featureTransformer().threatWeights[feature * kL1Size];
            for (i32 i = 0; i < kL1Size; ++i) {
                acc[i] += start[i];
            }
        };

        const auto occ = boards.bbs().occupancy();

        auto bb = occ & ~boards.bbs().kings();
        while (bb) {
            const auto from = bb.popLowestSquare();
            const auto piece = boards.pieceOn(from);

            auto threats = occ & attacks::getAttacks(piece, from, occ) & ~boards.bbs().kings();
            while (threats) {
                const auto to = threats.popLowestSquare();
                const auto attacked = boards.pieceOn(to);
                const auto feature = nnue::features::threats::featureIndex(c, king, piece, from, attacked, to);
                if (feature < nnue::features::threats::kTotalThreatFeatures) {
                    activateFeature(feature);
                }
            }
        }
    }

    namespace {
        namespace geometry = nnue::features::threats::geometry;
        using UpdatedThreat = nnue::features::psq::UpdatedThreat;

#if SP_HAS_VBMI2
        static_assert(sizeof(UpdatedThreat) == sizeof(u32));
        static_assert(offsetof(UpdatedThreat, attacker) == 0 * sizeof(u8));
        static_assert(offsetof(UpdatedThreat, attackerSq) == 1 * sizeof(u8));
        static_assert(offsetof(UpdatedThreat, attacked) == 2 * sizeof(u8));
        static_assert(offsetof(UpdatedThreat, attackedSq) == 3 * sizeof(u8));

        template <bool kAdd, bool kOutgoing>
        inline void pushFocusThreatFeatures(
            NnueUpdates& updates,
            geometry::Vector indexes, // List of square indexes
            geometry::Vector rays,    // List of pieces on those squares as indexed by indexes
            geometry::Bitrays br,     // Bitrays where bit set is a piece attacked/being attacked by focus square
            Piece piece,              // Piece on the focus square
            Square sq                 // The focus square
        ) {
            // Create the (piecea, squarea, pieceb, squareb) tuples.
            // Whether pair1 or pair2 is paira or pairb is determined by kOutgoing.

            // clang-format off
            const auto pair2Shuffle = _mm512_set_epi8(
                79, 15, 79, 15, 78, 14, 78, 14, 77, 13, 77, 13, 76, 12, 76, 12, 75, 11, 75, 11,
                74, 10, 74, 10, 73, 9, 73, 9, 72, 8, 72, 8, 71, 7, 71, 7, 70, 6, 70, 6, 69, 5,
                69, 5, 68, 4, 68, 4, 67, 3, 67, 3, 66, 2, 66, 2, 65, 1, 65, 1, 64, 0, 64, 0
            );
            // clang-format on

            // Focus pair
            const auto pair1 = _mm512_set1_epi16(static_cast<i16>(piece.idx() | (sq.idx() << 8)));

            // Non-focus pair
            const auto pair2Sq = _mm512_maskz_compress_epi8(br, indexes.raw);
            const auto pair2Piece = _mm512_maskz_compress_epi8(br, rays.raw);
            const auto pair2 = _mm512_permutex2var_epi8(pair2Piece, pair2Shuffle, pair2Sq);

            // Select which is the attacker and which is the victim.
            constexpr u64 mask = kOutgoing ? 0xCCCCCCCCCCCCCCCC : 0x3333333333333333;
            const auto vector = _mm512_mask_mov_epi8(pair1, mask, pair2);

            (kAdd ? updates.threatsAdded : updates.threatsRemoved).unsafeWrite([&](UpdatedThreat* ptr) {
                _mm512_storeu_si512(ptr, vector);
                return std::popcount(br);
            });
        }

        template <bool kAdd>
        inline void pushDiscoveredThreatFeatures(
            NnueUpdates& updates,
            geometry::Vector indexes, // Squares
            geometry::Vector rays,    // Pieces
            geometry::Bitrays sliders,
            geometry::Bitrays victims
        ) {
            const auto count = std::popcount(victims);
            assert(std::popcount(victims) == std::popcount(sliders));

            // Create the (piece1, square1, piece2, square2) tuples.

            const auto p1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, rays.raw));
            const auto sq1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, indexes.raw));
            const auto p2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, rays.flip().raw));
            const auto sq2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, indexes.flip().raw));

            const auto pair1 = _mm_unpacklo_epi8(p1, sq1);
            const auto pair2 = _mm_unpacklo_epi8(p2, sq2);

            const auto tuple1 = _mm_unpacklo_epi16(pair1, pair2);
            const auto tuple2 = _mm_unpackhi_epi16(pair1, pair2);

            // Opposite polarity:
            // - Adding focus piece removes x-ray threats (a.k.a. slider threat retraction)
            // - Removing focus piece adds x-ray threats (a.k.a. slider threat extension)
            (kAdd ? updates.threatsRemoved : updates.threatsAdded).unsafeWrite([&](UpdatedThreat* ptr) {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 0, tuple1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 1, tuple2);
                return count;
            });
        }
#else
        template <bool kAdd, bool kOutgoing>
        inline void pushFocusThreatFeatures(
            NnueUpdates& updates,
            geometry::Vector indexes, // List of square indexes
            geometry::Vector rays,    // List of pieces on those squares as indexed by indexes
            geometry::Bitrays br,     // Bitrays where bit set is a piece attacked/being attacked by focus square
            Piece piece,              // Piece on the focus square
            Square sq                 // The focus square
        ) {
            std::array<Piece, 64> others;
            std::memcpy(others.data(), &rays, sizeof(others));
            std::array<Square, 64> otherSqs;
            std::memcpy(otherSqs.data(), &indexes, sizeof(otherSqs));

            for (; br; br = util::resetLsb(br)) {
                const auto i = util::ctz(br);

                const auto other = others[i];
                const auto otherSq = otherSqs[i];

                const auto attacker = kOutgoing ? piece : other;
                const auto attackerSq = kOutgoing ? sq : otherSq;
                const auto attacked = kOutgoing ? other : piece;
                const auto attackedSq = kOutgoing ? otherSq : sq;

                if constexpr (kAdd) {
                    updates.addThreatFeature(attacker, attackerSq, attacked, attackedSq);
                } else {
                    updates.removeThreatFeature(attacker, attackerSq, attacked, attackedSq);
                }
            }
        }

        template <bool kAdd>
        inline void pushDiscoveredThreatFeatures(
            NnueUpdates& updates,
            geometry::Vector indexes, // Squares
            geometry::Vector rays,    // Pieces
            geometry::Bitrays sliders,
            geometry::Bitrays victims
        ) {
            std::array<Piece, 64> pieces;
            std::memcpy(pieces.data(), &rays, sizeof(pieces));
            std::array<Square, 64> squares;
            std::memcpy(squares.data(), &indexes, sizeof(squares));

            for (; sliders; sliders = util::resetLsb(sliders), victims = util::resetLsb(victims)) {
                const auto slider = util::ctz(sliders);
                const auto victim = util::ctz(victims);

                const auto attacker = pieces[slider];
                const auto attackerSq = squares[slider];
                const auto attacked = pieces[(victim + 32) % 64];
                const auto attackedSq = squares[(victim + 32) % 64];

                // Opposite polarity:
                // - Adding focus piece removes x-ray threats (a.k.a. slider threat retraction)
                // - Removing focus piece adds x-ray threats (a.k.a. slider threat extension)
                if constexpr (kAdd) {
                    updates.removeThreatFeature(attacker, attackerSq, attacked, attackedSq);
                } else {
                    updates.addThreatFeature(attacker, attackerSq, attacked, attackedSq);
                }
            }

            assert(!sliders && !victims);
        }
#endif
    } // namespace

    template void updatePieceThreatsOnChange<false>(NnueUpdates&, const PositionBoards&, Piece, Square);
    template void updatePieceThreatsOnChange<true>(NnueUpdates&, const PositionBoards&, Piece, Square);

    template <bool kAdd>
    void updatePieceThreatsOnChange(NnueUpdates& updates, const PositionBoards& boards, Piece piece, Square sq) {
        using namespace nnue::features::threats;

        // Generate a list of indexes and ray array for a given focus square sq.
        const auto permutation = geometry::permutationFor(sq);
        const auto [rays, bits] = geometry::permuteMailbox(permutation, boards.mailbox());

        // Determine all threats relative to the focus square sq.
        const auto closest = geometry::closestOccupied(bits);
        const auto outgoingThreats = geometry::outgoingThreats(piece, closest);
        const auto incomingAttackers = geometry::incomingAttackers(bits, closest);
        const auto incomingSliders = geometry::incomingSliders(bits, closest);

        // Push all focus square relative threats.
        pushFocusThreatFeatures<kAdd, true>(updates, permutation.indexes, rays, outgoingThreats, piece, sq);
        pushFocusThreatFeatures<kAdd, false>(updates, permutation.indexes, rays, incomingAttackers, piece, sq);

        // Discover threat updates from sliders whose threats needs to be extended (if focus piece removed)
        // or retracted (if focus piece added).
        // A valid threat is when one ray has a slider on it, with a vicitim on the opposite ray.
        // For example, a bishop on NW ray and blocker on the SE ray.
        // Here we just detect all valid discovered threats.
        const auto victimMask = std::rotr(closest & 0xFEFEFEFEFEFEFEFE, 32);
        const auto valid = geometry::rayFill(victimMask) & geometry::rayFill(incomingSliders);

        pushDiscoveredThreatFeatures<kAdd>(
            updates,
            permutation.indexes,
            rays,
            incomingSliders & valid,
            victimMask & valid
        );
    }

    void updatePieceThreatsOnMutate(
        NnueUpdates& updates,
        const PositionBoards& boards,
        Piece oldPiece,
        Piece newPiece,
        Square sq
    ) {
        using namespace nnue::features::threats;

        // Generate a list of indexes and ray array for a given focus square sq.
        const auto permutation = geometry::permutationFor(sq);
        const auto [rays, bits] = geometry::permuteMailbox(permutation, boards.mailbox());

        // Determine all threats relative to the focus square sq.
        const auto closest = geometry::closestOccupied(bits);
        const auto oldOutgoingThreats = geometry::outgoingThreats(oldPiece, closest);
        const auto newOutgoingThreats = geometry::outgoingThreats(newPiece, closest);
        const auto incomingAttackers = geometry::incomingAttackers(bits, closest);

        // Push all focus square relative threats.
        pushFocusThreatFeatures<false, true>(updates, permutation.indexes, rays, oldOutgoingThreats, oldPiece, sq);
        pushFocusThreatFeatures<true, true>(updates, permutation.indexes, rays, newOutgoingThreats, newPiece, sq);
        pushFocusThreatFeatures<false, false>(updates, permutation.indexes, rays, incomingAttackers, oldPiece, sq);
        pushFocusThreatFeatures<true, false>(updates, permutation.indexes, rays, incomingAttackers, newPiece, sq);
    }

    void updatePieceThreatsOnMove(
        NnueUpdates& updates,
        const PositionBoards& boards,
        Piece oldPiece,
        Square src,
        Piece newPiece,
        Square dst
    ) {
        using namespace nnue::features::threats;

        const auto srcPerm = geometry::permutationFor(src);
        const auto dstPerm = geometry::permutationFor(dst);
        const auto [srcRays, srcBits] = geometry::permuteMailbox(srcPerm, boards.mailbox(), dst);
        const auto [dstRays, dstBits] = geometry::permuteMailbox(dstPerm, boards.mailbox());

        const auto srcClosest = geometry::closestOccupied(srcBits);
        const auto dstClosest = geometry::closestOccupied(dstBits);
        const auto srcOutgoingThreats = geometry::outgoingThreats(oldPiece, srcClosest);
        const auto dstOutgoingThreats = geometry::outgoingThreats(newPiece, dstClosest);
        const auto srcIncomingAttackers = geometry::incomingAttackers(srcBits, srcClosest);
        const auto dstIncomingAttackers = geometry::incomingAttackers(dstBits, dstClosest);
        const auto srcIncomingSliders = geometry::incomingSliders(srcBits, srcClosest);
        const auto dstIncomingSliders = geometry::incomingSliders(dstBits, dstClosest);

        pushFocusThreatFeatures<false, true>(updates, srcPerm.indexes, srcRays, srcOutgoingThreats, oldPiece, src);
        pushFocusThreatFeatures<true, true>(updates, dstPerm.indexes, dstRays, dstOutgoingThreats, newPiece, dst);
        pushFocusThreatFeatures<false, false>(updates, srcPerm.indexes, srcRays, srcIncomingAttackers, oldPiece, src);
        pushFocusThreatFeatures<true, false>(updates, dstPerm.indexes, dstRays, dstIncomingAttackers, newPiece, dst);

        const auto srcVictimMask = std::rotr(srcClosest & 0xFEFEFEFEFEFEFEFE, 32);
        const auto dstVictimMask = std::rotr(dstClosest & 0xFEFEFEFEFEFEFEFE, 32);
        const auto srcValid = geometry::rayFill(srcVictimMask) & geometry::rayFill(srcIncomingSliders);
        const auto dstValid = geometry::rayFill(dstVictimMask) & geometry::rayFill(dstIncomingSliders);

        pushDiscoveredThreatFeatures<false>(
            updates,
            srcPerm.indexes,
            srcRays,
            srcIncomingSliders & srcValid,
            srcVictimMask & srcValid
        );
        pushDiscoveredThreatFeatures<true>(
            updates,
            dstPerm.indexes,
            dstRays,
            dstIncomingSliders & dstValid,
            dstVictimMask & dstValid
        );
    }
} // namespace stormphrax::eval
