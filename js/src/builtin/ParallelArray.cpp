/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsobj.h"
#include "jsprf.h"

#include "jsobjinlines.h"
#include "jsarrayinlines.h"

using namespace js;

enum {
    JSSLOT_PA_LENGTH = 0,
    JSSLOT_PA_BUFFER,
    JSSLOT_PA_MAX
};

static bool
IsParallelArray(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&ParallelArrayClass);
}

inline uint32_t
GetLength(JSObject *obj)
{
    return uint32_t(obj->getSlot(JSSLOT_PA_LENGTH).toInt32());
}

inline JSObject *
GetBuffer(JSObject *obj)
{
    return &(obj->getSlot(JSSLOT_PA_BUFFER).toObject());
}

static JSObject *
NewParallelArray(JSContext *cx, JSObject *buffer, uint32_t length)
{
    JSObject *result = NewBuiltinClassInstance(cx, &ParallelArrayClass);
    if (!result)
        return NULL;

    result->setSlot(JSSLOT_PA_LENGTH, Int32Value(int32_t(length)));
    result->setSlot(JSSLOT_PA_BUFFER, ObjectValue(*buffer));

    return result;
}

static bool
ParallelArray_get_impl(JSContext *cx, CallArgs args)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "ParallelArray.get", "0", "s");
        return false;
    }

    RootedObject obj(cx, &args.thisv().toObject());

    uint32_t index;
    if (!ToUint32(cx, args[0], &index))
        return false;

    uint32_t length = GetLength(obj);

    if (index >= length)
        return false;

    args.rval().set(GetBuffer(obj)->getDenseArrayElement(index));

    return true;
}

static JSBool
ParallelArray_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ParallelArray_get_impl, args);
}

static JSBool
ParallelArray_build(JSContext *cx, uint32_t length, const Value &thisv, JSObject *elementalFun,
                    bool passElement, unsigned extrasc, Value *extrasp, MutableHandleValue vp)
{
    /* create data store for results */
    RootedObject buffer(cx, NewDenseAllocatedArray(cx, length));
    if (!buffer)
        return false;

    buffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* grab source buffer if we need to pass elements */
    RootedObject srcBuffer(cx);
    if (passElement)
        srcBuffer = &(thisv.toObject().getSlot(JSSLOT_PA_BUFFER).toObject());

    /* prepare call frame on stack */
    InvokeArgsGuard args;
    cx->stack.pushInvokeArgs(cx, extrasc + 1, &args);

    RootedObject extra(cx);
    RootedValue v(cx);
    for (uint32_t i = 0; i < length; i++) {
        args.setCallee(ObjectValue(*elementalFun));
        if (passElement)
            args[0] = srcBuffer->getDenseArrayElement(i);
        else
            args[0].setNumber(i);

        /* set value of this */
        args.thisv() = thisv;

        /* set extra arguments */
        for (unsigned j = 0; j < extrasc; j++) {
            extra = &extrasp[j].toObject();

            if (!extra->getElement(cx, extra, i, &v))
                return false;
            args[j + 1] = v;
        }

        /* call */
        if (!Invoke(cx, args))
            return false;

        /* set result element */
        buffer->setDenseArrayElementWithType(cx, i, args.rval());
    }

    /* create ParallelArray wrapper class */
    RootedObject result(cx, NewParallelArray(cx, buffer, length));
    if (!result)
        return false;

    vp.setObject(*result);
    return true;
}

static JSBool
ParallelArray_construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "ParallelArray", "0", "s");
        return false;
    }

    if (args.length() == 1) {
        /* first case: init using an array value */
        if (!args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
            return false;
        }

        RootedObject src(cx, &(args[0].toObject()));

        uint32_t srcLen;
        if (!js_GetLengthProperty(cx, src, &srcLen))
            return false;

        /* allocate buffer for result */
        RootedObject buffer(cx, NewDenseAllocatedArray(cx, srcLen));
        if (!buffer)
            return false;

        buffer->ensureDenseArrayInitializedLength(cx, srcLen, 0);

        RootedValue elem(cx);
        for (uint32_t i = 0; i < srcLen; i++) {
            if (src->isDenseArray() && (i < src->getDenseArrayInitializedLength())) {
                /* dense array case */
                elem = src->getDenseArrayElement(i);
                if (elem.isMagic(JS_ARRAY_HOLE))
                    elem.setUndefined();
            } else {
                /* generic case */
                if (!src->getElement(cx, src, i, &elem))
                    return false;
            }

            buffer->setDenseArrayElementWithType(cx, i, elem);
        }

        RootedObject result(cx, NewParallelArray(cx, buffer, srcLen));
        if (!result)
            return false;

        args.rval().setObject(*result);
        return true;
    }

    /* second case: init using length and function */
    /* extract first argument, the length */
    uint32_t length;
    if (!ToUint32(cx, args[0], &length))
        return false;

    /* extract second argument, the elemental function */
    RootedObject elementalFun(cx, ValueToCallable(cx, &args[1]));
    if (!elementalFun)
        return false;

    /* use build with |this| set to |undefined| */
    return ParallelArray_build(cx, length, UndefinedValue(), elementalFun, false, 0, NULL, args.rval());
}

template <bool IsMap>
static bool
MapOrCombine(JSContext *cx, CallArgs args)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             (IsMap ? "map" : "combine"), "0", "s");
        return false;
    }

    RootedObject obj(cx, &args.thisv().toObject());

    /* extract first argument, the elemental function */
    RootedObject elementalFun(cx, ValueToCallable(cx, &args[0]));
    if (!elementalFun)
        return false;

    /* check extra arguments for map to be objects */
    if (IsMap && (args.length() > 1)) {
        for (unsigned i = 1; i < args.length(); i++) {
            if (!args[i].isObject()) {
                char buffer[4];
                JS_snprintf(buffer, 4, "%d", i+1);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_MAP_INVALID_ARG,
                                     buffer);

                return false;
            }
        }
    }

    return ParallelArray_build(cx, GetLength(obj), ObjectValue(*obj), elementalFun, IsMap,
                               (IsMap
                                ? args.length() - 1
                                : 0),
                               ((args.length() > 1)
                                ? &(args[1])
                                : NULL),
                               args.rval());
}

static JSBool
ParallelArray_map(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, MapOrCombine<true>, args);
}

static JSBool
ParallelArray_combine(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, MapOrCombine<false>, args);
}

template <bool IsScan>
static bool
ScanOrReduce(JSContext *cx, CallArgs args)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             (IsScan ? "scan" : "reduce"), "0", "s");
        return false;
    }

    RootedObject obj(cx, &args.thisv().toObject());
    uint32_t length = GetLength(obj);

    RootedObject result(cx);
    RootedObject resBuffer(cx);
    if (IsScan) {
        /* create data store for results */
        resBuffer = NewDenseAllocatedArray(cx, length);
        if (!resBuffer)
            return false;

        resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

        /* create ParallelArray wrapper class */
        result = NewParallelArray(cx, resBuffer, length);
        if (!result)
            return false;
    }

    /* extract first argument, the elemental function */
    RootedObject elementalFun(cx, ValueToCallable(cx, &args[0]));
    if (!elementalFun)
        return false;

    /* special case of empty arrays */
    if (length == 0) {
        args.rval().set(IsScan ? ObjectValue(*result) : UndefinedValue());
        return true;
    }

    RootedObject buffer(cx, GetBuffer(obj));

    Value accu = buffer->getDenseArrayElement(0);
    if (IsScan)
        resBuffer->setDenseArrayElementWithType(cx, 0, accu);

    /* prepare call frame on stack */
    InvokeArgsGuard ag;
    if (!cx->stack.pushInvokeArgs(cx, 2, &ag))
        return false;

    for (uint32_t i = 1; i < length; i++) {
        /* fill frame with current values */
        ag.setCallee(ObjectValue(*elementalFun));
        ag[0] = accu;
        ag[1] = buffer->getDenseArrayElement(i);

        /* We set |this| inside of the kernel to the |this| we were invoked on. */
        /* This is a random choice, as we need some value here. */
        ag.thisv() = args.thisv();

        /* call */
        if (!Invoke(cx, ag))
            return false;

        /* remember result for next round */
        accu = ag.rval().get();
        if (IsScan)
            resBuffer->setDenseArrayElementWithType(cx, i, accu);
    }

    args.rval().set(IsScan ? ObjectValue(*result) : accu);

    return true;
}

static JSBool
ParallelArray_scan(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ScanOrReduce<true>, args);
}

static JSBool
ParallelArray_reduce(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ScanOrReduce<false>, args);
}


static bool
ParallelArray_filter_impl(JSContext *cx, CallArgs args)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "filter", "0", "s");
        return false;
    }

    RootedObject obj(cx, &args.thisv().toObject());

    /* extract first argument, the elemental function */
    RootedObject elementalFun(cx, ValueToCallable(cx, &args[0]));
    if (!elementalFun)
        return false;

    RootedObject buffer(cx, GetBuffer(obj));
    uint32_t length = GetLength(obj);

    /* We just assume the length of the input as the length of the output. */
    RootedObject resBuffer(cx, NewDenseAllocatedArray(cx, length));
    if (!resBuffer)
        return false;

    resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* prepare call frame on stack */
    InvokeArgsGuard frame;
    cx->stack.pushInvokeArgs(cx, 1, &frame);

    uint32_t pos = 0;
    for (uint32_t i = 0; i < length; i++) {
        frame.setCallee(ObjectValue(*elementalFun));
        frame[0].setNumber(i);
        frame.thisv() = ObjectValue(*obj);

        /* call */
        if (!Invoke(cx, frame))
            return false;

        if (ToBoolean(frame.rval()))
            resBuffer->setDenseArrayElementWithType(cx, pos++, buffer->getDenseArrayElement(i));
    }

    /* shrink the array to the proper size */
    resBuffer->setArrayLength(cx, pos);

    /* create ParallelArray wrapper class */
    RootedObject result(cx, NewParallelArray(cx, resBuffer, pos));
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}

static JSBool
ParallelArray_filter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ParallelArray_filter_impl, args);
}

static bool
ParallelArray_scatter_impl(JSContext *cx, CallArgs args)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "scatter", "0", "s");
        return false;
    }

    RootedObject obj(cx, &args.thisv().toObject());
    RootedObject buffer(cx, GetBuffer(obj));

    /* grab the scatter vector */
    if (!args[0].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_INVALID_VEC);
        return false;
    }
    RootedObject targets(cx, &args[0].toObject());

    uint32_t scatterLen;
    if (!JS_GetArrayLength(cx, targets, &scatterLen))
        return false;

    if (scatterLen > GetLength(obj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_INVALID_VEC);
        return false;
    }

    /* next, default value */
    RootedValue defValue(cx, UndefinedValue());
    if (args.length() >= 2)
        defValue = args[1];

    RootedObject conflictFun(cx);
    /* conflict resolution function */
    if ((args.length() >= 3) && !args[2].isUndefined()) {
        conflictFun = ValueToCallable(cx, &args[2]);
        if (!conflictFun)
            return false;
    }

    /* optional length */
    uint32_t length;
    if (args.length() >= 4) {
        if (!ToUint32(cx, args[3], &length))
            return false;
    } else {
        /* we assume the source's length */
        length = GetLength(obj);
    }

    /* allocate space for the result */
    RootedObject resBuffer(cx, NewDenseAllocatedArray(cx, length));
    if (!resBuffer)
        return false;

    resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* iterate over the scatter vector */
    for (uint32_t i = 0; i < scatterLen; i++) {
        /* read target index */
        RootedValue elem(cx);
        if (!targets->getElement(cx, targets, i, &elem))
            return false;

        uint32_t targetIdx;
        if (!ToUint32(cx, elem, &targetIdx))
            return false;

        if (targetIdx >= length) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_BNDS);
            return false;
        }

        /* read current value */
        RootedValue readV(cx, buffer->getDenseArrayElement(i));

        RootedValue previous(cx, resBuffer->getDenseArrayElement(targetIdx));

        if (!previous.get().isMagic(JS_ARRAY_HOLE)) {
            if (conflictFun) {
                /* we have a conflict, so call the resolution function to resovle it */
                InvokeArgsGuard ag;
                if (!cx->stack.pushInvokeArgs(cx, 2, &ag))
                    return false;
                ag.setCallee(ObjectValue(*conflictFun));
                ag[0] = readV;
                ag[1] = previous;

                /* random choice for |this| */
                ag.thisv() = args.thisv();

                if (!Invoke(cx, ag))
                    return false;

                readV = ag.rval();
            } else {
                /* no conflict function defined, yet we have a conflict -> fail */
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_CONFLICT);
                return false;
            }
        }

        /* write back */
        resBuffer->setDenseArrayElementWithType(cx, targetIdx, readV);
    }

    /* fill holes */
    for (uint32_t i = 0; i < length; i++) {
        if (resBuffer->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE))
            resBuffer->setDenseArrayElementWithType(cx, i, defValue);
    }

    /* create ParallelArray wrapper class */
    RootedObject result(cx, NewParallelArray(cx, resBuffer, length));
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}

static JSBool
ParallelArray_scatter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ParallelArray_scatter_impl, args);
}

static bool
ParallelArray_toString_impl(JSContext *cx, CallArgs args)
{
    RootedValue callable(cx);
    RootedObject obj(cx, &args.thisv().toObject());
    RootedObject buffer(cx, GetBuffer(obj));
    RootedId id(cx, NameToId(cx->runtime->atomState.toStringAtom->asPropertyName()));

    if (!GetMethod(cx, buffer, id, 0, &callable))
        return false;

    RootedValue rval(cx);
    if (!Invoke(cx, ObjectOrNullValue(buffer), callable, args.length(), args.array(), rval.address()))
        return false;

    args.rval().set(rval);
    return true;
}

static JSBool
ParallelArray_toString(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsParallelArray, ParallelArray_toString_impl, args);
}

static JSBool
ParallelArray_length_getter(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    /* we do not support prototype chaining for now */
    if (obj->getClass() == &ParallelArrayClass) {
        /* return the length of the ParallelArray object */
        vp.setNumber(GetLength(obj));
    } else {
        /* return the length of the prototype's function object */
        JS_ASSERT(obj->getClass() == &ParallelArrayProtoClass);
        vp.setInt32(0);
    }

    return true;
}

/* Checks whether the index is in range. We guarantee dense arrays. */
static inline bool
IsDenseArrayIndex(JSObject *obj, uint32_t index)
{
    JS_ASSERT(obj->isDenseArray());

    return index < obj->getDenseArrayInitializedLength();
}

/* checks whether id is an index */
static inline bool
IsDenseArrayId(JSContext *cx, JSObject *obj, HandleId id)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t i;
    return (js_IdIsIndex(id, &i) && IsDenseArrayIndex(obj, i));
}

static JSBool
ParallelArray_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id, MutableHandleObject objp,
                            MutableHandleShape propp)
{
    RootedObject buffer(cx, GetBuffer(obj));

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
        IsDenseArrayId(cx, buffer, id))
    {
        MarkNonNativePropertyFound(obj, propp);
        objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return proto->lookupGeneric(cx, id, objp, propp);

    objp.set(NULL);
    propp.set(NULL);
    return true;
}

static JSBool
ParallelArray_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, MutableHandleObject objp,
                             MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_lookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
ParallelArray_lookupElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleObject objp,
                            MutableHandleShape propp)
{
    if (IsDenseArrayIndex(GetBuffer(obj), index)) {
        MarkNonNativePropertyFound(obj, propp);
        objp.set(obj);
        return true;
    }

    JSObject *proto = obj->getProto();
    if (proto)
        return proto->lookupElement(cx, index, objp, propp);

    objp.set(NULL);
    propp.set(NULL);
    return true;
}

static JSBool
ParallelArray_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, MutableHandleObject objp,
                            MutableHandleShape propp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_lookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
ParallelArray_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                         MutableHandleValue vp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        vp.setNumber(GetLength(obj));
        return true;
    }

    RootedObject buffer(cx, GetBuffer(obj));
    if (IsDenseArrayId(cx, buffer, id))
        return buffer->getGeneric(cx, receiver, id, vp);

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return proto->getGeneric(cx, receiver, id, vp);

    vp.setUndefined();
    return true;
}

static JSBool
ParallelArray_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                          HandlePropertyName name, MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_getGeneric(cx, obj, receiver, id, vp);
}

static JSBool
ParallelArray_getElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                         MutableHandleValue vp)
{
    RootedObject buffer(cx, GetBuffer(obj));
    if (IsDenseArrayIndex(buffer, index)) {
        vp.set(buffer->getDenseArrayElement(index));
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return proto->getElement(cx, receiver, index, vp);

    vp.set(UndefinedValue());
    return true;
}

static JSBool
ParallelArray_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                         MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_getGeneric(cx, obj, receiver, id, vp);
}

static JSBool
ParallelArray_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                            JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_defineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
                             JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
ParallelArray_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                            PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue value,
                            PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
ParallelArray_setGeneric(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                          MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_setGeneric(cx, obj, id, vp, strict);
}

static JSBool
ParallelArray_setElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_setGeneric(cx, obj, id, vp, strict);
}

static JSBool
ParallelArray_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY;
    } else {
        /* this must be an element then */
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE;
    }

    return true;
}

static JSBool
ParallelArray_getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_getGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
ParallelArray_getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE;
    return true;
}

static JSBool
ParallelArray_getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_getGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
ParallelArray_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_setGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
ParallelArray_setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_setGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
ParallelArray_deleteGeneric(JSContext *cx, HandleObject obj, HandleId id,
                            MutableHandleValue vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);
    return false;
}

static JSBool
ParallelArray_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                             MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx, NameToId(name));
    return ParallelArray_deleteGeneric(cx, obj, id, vp, strict);
}

static JSBool
ParallelArray_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                            MutableHandleValue vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                            MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return ParallelArray_deleteGeneric(cx, obj, id, vp, strict);
}

static JSBool
ParallelArray_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                        Value *statep, jsid *idp)
{
    /*
     * Iteration is "length" (if JSENUMERATE_INIT_ALL), then [0, length).
     * *statep is JSVAL_TRUE if enumerating "length" and
     * JSVAL_TO_INT(index) when enumerating index.
     */
    switch (enum_op) {
      case JSENUMERATE_INIT_ALL:
        *statep = BooleanValue(true);
        if (idp)
            *idp = ::INT_TO_JSID(GetLength(obj) + 1);
        break;

      case JSENUMERATE_INIT:
        *statep = Int32Value(0);
        if (idp)
            *idp = ::INT_TO_JSID(GetLength(obj));
        break;

      case JSENUMERATE_NEXT:
        if (statep->isTrue()) {
            *idp = AtomToId(cx->runtime->atomState.lengthAtom);
            *statep = Int32Value(0);
        } else {
            uint32_t index = statep->toInt32();
            if (index < GetLength(obj)) {
                *idp = ::INT_TO_JSID(index);
                *statep= Int32Value(index + 1);
            } else {
                JS_ASSERT(index == GetLength(obj));
                *statep = NullValue();
            }
        }
        break;

      case JSENUMERATE_DESTROY:
        *statep = NullValue();
        break;
    }

    return true;
}

static void
ParallelArray_trace(JSTracer *trc, JSObject *obj)
{
    gc::MarkSlot(trc, &obj->getSlotRef(JSSLOT_PA_LENGTH), "parallel-array-length");
    gc::MarkSlot(trc, &obj->getSlotRef(JSSLOT_PA_BUFFER), "parallel-array-buffer");
}

Class js::ParallelArrayProtoClass = {
    "ParallelArray",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class js::ParallelArrayClass = {
    "ParallelArray",
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_PA_MAX) | JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray) | Class::NON_NATIVE,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* reserved0   */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    ParallelArray_trace,     /* trace */
    JS_NULL_CLASS_EXT,
    {
        ParallelArray_lookupGeneric,
        ParallelArray_lookupProperty,
        ParallelArray_lookupElement,
        ParallelArray_lookupSpecial,
        ParallelArray_defineGeneric,
        ParallelArray_defineProperty,
        ParallelArray_defineElement,
        ParallelArray_defineSpecial,
        ParallelArray_getGeneric,
        ParallelArray_getProperty,
        ParallelArray_getElement,
        NULL,       /* getElementIfPresent */
        ParallelArray_getSpecial,
        ParallelArray_setGeneric,
        ParallelArray_setProperty,
        ParallelArray_setElement,
        ParallelArray_setSpecial,
        ParallelArray_getGenericAttributes,
        ParallelArray_getPropertyAttributes,
        ParallelArray_getElementAttributes,
        ParallelArray_getSpecialAttributes,
        ParallelArray_setGenericAttributes,
        ParallelArray_setPropertyAttributes,
        ParallelArray_setElementAttributes,
        ParallelArray_setSpecialAttributes,
        ParallelArray_deleteProperty,
        ParallelArray_deleteElement,
        ParallelArray_deleteSpecial,
        ParallelArray_enumerate,
        NULL,       /* typeof         */
        NULL,       /* thisObject     */
        NULL,       /* clear          */
    }
};

static JSFunctionSpec parallel_array_methods[] = {
    JS_FN("get",                 ParallelArray_get,            1, 0),
    JS_FN("map",                 ParallelArray_map,            1, 0),
    JS_FN("combine",             ParallelArray_combine,        1, 0),
    JS_FN("scan",                ParallelArray_scan,           1, 0),
    JS_FN("reduce",              ParallelArray_reduce,         1, 0),
    JS_FN("filter",              ParallelArray_filter,         1, 0),
    JS_FN("scatter",             ParallelArray_scatter,        1, 0),
    JS_FN(js_toString_str,       ParallelArray_toString,       0, 0),
    JS_FN(js_toLocaleString_str, ParallelArray_toString,       0, 0),
    JS_FN(js_toSource_str,       ParallelArray_toString,       0, 0),
    JS_FS_END
};

JSObject *
js_InitParallelArrayClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    GlobalObject *global = &obj->asGlobal();

    RootedObject parallelArrayProto(cx, global->createBlankPrototype(cx, &ParallelArrayProtoClass));
    if (!parallelArrayProto)
        return NULL;
    /* define the length property */
    RootedId lengthId(cx, AtomToId(cx->runtime->atomState.lengthAtom));

    parallelArrayProto->addProperty(cx, lengthId, ParallelArray_length_getter, NULL,
                                    SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_READONLY, 0, 0);

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, ParallelArray_construct, CLASS_NAME(cx, ParallelArray), 0);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, parallelArrayProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, parallelArrayProto, NULL, parallel_array_methods))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_ParallelArray, ctor, parallelArrayProto))
        return NULL;
    return parallelArrayProto;
}

