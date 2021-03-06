/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_include_safepoints_h_
#define jsion_include_safepoints_h_

#include "Registers.h"
#include "CompactBuffer.h"
#include "BitSet.h"

#include "shared/Assembler-shared.h"

namespace js {
namespace ion {

struct SafepointNunboxEntry;
class LAllocation;

static const uint32 INVALID_SAFEPOINT_OFFSET = uint32(-1);

class SafepointWriter
{
    CompactBufferWriter stream_;
    BitSet *frameSlots_;

  public:
    bool init(uint32 slotCount);

    // A safepoint entry is written in the order these functions appear.
    uint32 startEntry();
    void writeOsiCallPointOffset(uint32 osiPointOffset);
    void writeGcRegs(GeneralRegisterSet gc, GeneralRegisterSet spilled);
    void writeGcSlots(uint32 nslots, uint32 *slots);
    void writeValueSlots(uint32 nslots, uint32 *slots);
    void writeNunboxParts(uint32 nentries, SafepointNunboxEntry *entries);
    void endEntry();

    size_t size() const {
        return stream_.length();
    }
    const uint8 *buffer() const {
        return stream_.buffer();
    }
};

class SafepointReader
{
    CompactBufferReader stream_;
    uint32 frameSlots_;
    uint32 currentSlotChunk_;
    uint32 currentSlotChunkNumber_;
    uint32 osiCallPointOffset_;
    GeneralRegisterSet gcSpills_;
    GeneralRegisterSet allSpills_;
    uint32 nunboxSlotsRemaining_;

  private:
    void advanceFromGcRegs();
    void advanceFromGcSlots();
    void advanceFromValueSlots();
    bool getSlotFromBitmap(uint32 *slot);

  public:
    SafepointReader(IonScript *script, const SafepointIndex *si);

    static CodeLocationLabel InvalidationPatchPoint(IonScript *script, const SafepointIndex *si);

    uint32 osiCallPointOffset() const {
        return osiCallPointOffset_;
    }
    GeneralRegisterSet gcSpills() const {
        return gcSpills_;
    }
    GeneralRegisterSet allSpills() const {
        return allSpills_;
    }
    uint32 osiReturnPointOffset() const;

    // Returns true if a slot was read, false if there are no more slots.
    bool getGcSlot(uint32 *slot);

    // Returns true if a slot was read, false if there are no more value slots.
    bool getValueSlot(uint32 *slot);

    // Returns true if a nunbox slot was read, false if there are no more
    // nunbox slots.
    bool getNunboxSlot(LAllocation *type, LAllocation *payload);
};

} // namespace ion
} // namespace js

#endif // jsion_include_safepoints_h_

