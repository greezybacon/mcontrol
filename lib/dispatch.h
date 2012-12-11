enum proxy_function_ids {
    TYPE__FIRST = 100,
    TYPE_mcConnect,
    TYPE_mcEventRegister,
    TYPE_mcEventUnregister,
    TYPE_mcFirmwareLoad,
    TYPE_mcHome,
    TYPE_mcJitter,
    TYPE_mcJitterUnits,
    TYPE_mcMicrocodeLoad,
    TYPE_mcMotorIsLocked,
    TYPE_mcMotorLock,
    TYPE_mcMotorUnlock,
    TYPE_mcMoveAbsolute,
    TYPE_mcMoveAbsoluteUnits,
    TYPE_mcMoveRelative,
    TYPE_mcMoveRelativeUnits,
    TYPE_mcPokeInteger,
    TYPE_mcPokeString,
    TYPE_mcPokeStringItem,
    TYPE_mcProfileGet,
    TYPE_mcProfileGetAccel,
    TYPE_mcProfileGetDeadband,
    TYPE_mcProfileGetDecel,
    TYPE_mcProfileGetInitialV,
    TYPE_mcProfileGetMaxSlip,
    TYPE_mcProfileGetMaxV,
    TYPE_mcProfileSet,
    TYPE_mcQueryInteger,
    TYPE_mcQueryIntegerUnits,
    TYPE_mcQueryString,
    TYPE_mcSearch,
    TYPE_mcStop,
    TYPE_mcTraceSubscribeAdd,
    TYPE_mcTraceSubscribeRemote,
    TYPE_mcTraceSubscribeRemove,
    TYPE_mcTraceUnsubscribeRemote,
    TYPE_mcUnitScaleGet,
    TYPE_mcUnitScaleSet,
    TYPE_mcUnitToMicroRevs,
    TYPE__LAST
};

extern void mcConnectImpl(request_message_t * message);
extern void mcEventRegisterImpl(request_message_t * message);
extern void mcEventUnregisterImpl(request_message_t * message);
extern void mcFirmwareLoadImpl(request_message_t * message);
extern void mcHomeImpl(request_message_t * message);
extern void mcJitterImpl(request_message_t * message);
extern void mcJitterUnitsImpl(request_message_t * message);
extern void mcMicrocodeLoadImpl(request_message_t * message);
extern void mcMotorIsLockedImpl(request_message_t * message);
extern void mcMotorLockImpl(request_message_t * message);
extern void mcMotorUnlockImpl(request_message_t * message);
extern void mcMoveAbsoluteImpl(request_message_t * message);
extern void mcMoveAbsoluteUnitsImpl(request_message_t * message);
extern void mcMoveRelativeImpl(request_message_t * message);
extern void mcMoveRelativeUnitsImpl(request_message_t * message);
extern void mcPokeIntegerImpl(request_message_t * message);
extern void mcPokeStringImpl(request_message_t * message);
extern void mcPokeStringItemImpl(request_message_t * message);
extern void mcProfileGetImpl(request_message_t * message);
extern void mcProfileGetAccelImpl(request_message_t * message);
extern void mcProfileGetDeadbandImpl(request_message_t * message);
extern void mcProfileGetDecelImpl(request_message_t * message);
extern void mcProfileGetInitialVImpl(request_message_t * message);
extern void mcProfileGetMaxSlipImpl(request_message_t * message);
extern void mcProfileGetMaxVImpl(request_message_t * message);
extern void mcProfileSetImpl(request_message_t * message);
extern void mcQueryIntegerImpl(request_message_t * message);
extern void mcQueryIntegerUnitsImpl(request_message_t * message);
extern void mcQueryStringImpl(request_message_t * message);
extern void mcSearchImpl(request_message_t * message);
extern void mcStopImpl(request_message_t * message);
extern void mcTraceSubscribeAddImpl(request_message_t * message);
extern void mcTraceSubscribeRemoteImpl(request_message_t * message);
extern void mcTraceSubscribeRemoveImpl(request_message_t * message);
extern void mcTraceUnsubscribeRemoteImpl(request_message_t * message);
extern void mcUnitScaleGetImpl(request_message_t * message);
extern void mcUnitScaleSetImpl(request_message_t * message);
extern void mcUnitToMicroRevsImpl(request_message_t * message);

#ifdef DISPATCH_INCLUDE_TABLE
struct dispatch_table {
    int type;
    void (*callable)(request_message_t * message);
} table[] = {
    { TYPE_mcConnect, mcConnectImpl },
    { TYPE_mcEventRegister, mcEventRegisterImpl },
    { TYPE_mcEventUnregister, mcEventUnregisterImpl },
    { TYPE_mcFirmwareLoad, mcFirmwareLoadImpl },
    { TYPE_mcHome, mcHomeImpl },
    { TYPE_mcJitter, mcJitterImpl },
    { TYPE_mcJitterUnits, mcJitterUnitsImpl },
    { TYPE_mcMicrocodeLoad, mcMicrocodeLoadImpl },
    { TYPE_mcMotorIsLocked, mcMotorIsLockedImpl },
    { TYPE_mcMotorLock, mcMotorLockImpl },
    { TYPE_mcMotorUnlock, mcMotorUnlockImpl },
    { TYPE_mcMoveAbsolute, mcMoveAbsoluteImpl },
    { TYPE_mcMoveAbsoluteUnits, mcMoveAbsoluteUnitsImpl },
    { TYPE_mcMoveRelative, mcMoveRelativeImpl },
    { TYPE_mcMoveRelativeUnits, mcMoveRelativeUnitsImpl },
    { TYPE_mcPokeInteger, mcPokeIntegerImpl },
    { TYPE_mcPokeString, mcPokeStringImpl },
    { TYPE_mcPokeStringItem, mcPokeStringItemImpl },
    { TYPE_mcProfileGet, mcProfileGetImpl },
    { TYPE_mcProfileGetAccel, mcProfileGetAccelImpl },
    { TYPE_mcProfileGetDeadband, mcProfileGetDeadbandImpl },
    { TYPE_mcProfileGetDecel, mcProfileGetDecelImpl },
    { TYPE_mcProfileGetInitialV, mcProfileGetInitialVImpl },
    { TYPE_mcProfileGetMaxSlip, mcProfileGetMaxSlipImpl },
    { TYPE_mcProfileGetMaxV, mcProfileGetMaxVImpl },
    { TYPE_mcProfileSet, mcProfileSetImpl },
    { TYPE_mcQueryInteger, mcQueryIntegerImpl },
    { TYPE_mcQueryIntegerUnits, mcQueryIntegerUnitsImpl },
    { TYPE_mcQueryString, mcQueryStringImpl },
    { TYPE_mcSearch, mcSearchImpl },
    { TYPE_mcStop, mcStopImpl },
    { TYPE_mcTraceSubscribeAdd, mcTraceSubscribeAddImpl },
    { TYPE_mcTraceSubscribeRemote, mcTraceSubscribeRemoteImpl },
    { TYPE_mcTraceSubscribeRemove, mcTraceSubscribeRemoveImpl },
    { TYPE_mcTraceUnsubscribeRemote, mcTraceUnsubscribeRemoteImpl },
    { TYPE_mcUnitScaleGet, mcUnitScaleGetImpl },
    { TYPE_mcUnitScaleSet, mcUnitScaleSetImpl },
    { TYPE_mcUnitToMicroRevs, mcUnitToMicroRevsImpl },
    { 0, NULL }
};
#endif

