package com.yuminaga.motonav;

/** One GATT attempt. Identity (not device address) rejects callbacks from closed attempts. */
final class BleSession<T> {
    enum Phase { CONNECTING, SERVICES, NOTIFY, MTU, AUTH, READY }
    private T owner;
    private Phase phase;
    private long deadline;

    void begin(T connection, long now) {
        owner = connection;
        phase = Phase.CONNECTING;
        deadline = now + 15000;
    }

    boolean owns(T connection) { return owner != null && owner == connection; }
    Phase phase() { return phase; }

    boolean move(T connection, Phase from, Phase to, long now, long timeout) {
        if (!owns(connection) || phase != from) return false;
        phase = to;
        deadline = timeout > 0 ? now + timeout : 0;
        return true;
    }

    boolean expired(long now) { return owner != null && deadline > 0 && now >= deadline; }

    void clear(T connection) {
        if (!owns(connection)) return;
        owner = null;
        phase = null;
        deadline = 0;
    }

    /** Only an explicit successful U reply or an authenticated capability-query response. */
    static boolean confirms(String message, boolean capabilityQuerySent) {
        return "U,1".equals(message) || (capabilityQuerySent && "CAP,BLD,1".equals(message));
    }
}
