import { useState } from "react";
import { Modal, Pressable, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import StatusChip from "./ui/StatusChip";

const MODE_INFO = {
  websocket:  { label: "WS",   tone: "ready" },
  mqtt:       { label: "MQTT", tone: "active" },
  polling:    { label: "REST", tone: "warning" },
  offline:    { label: "OFF",  tone: "danger" },
  connecting: { label: "…",   tone: "neutral" },
};

const ROLE_COLORS = {
  operator:     C.active,
  admin:        C.warning,
  manufacturer: C.settling,
};

const STATE_TONES = {
  IDLE:         "neutral",
  READY:        "ready",
  VALIDATING:   "active",
  FILLING_FAST: "active",
  FILLING_SLOW: "warning",
  SETTLING:     "settling",
  COMPLETE:     "ready",
  ABORTED:      "warning",
  FAULT:        "danger",
};

export default function StatusHeader({ status, streamMode, authRole, authUsername, onSwitchAccount }) {
  const [menuOpen, setMenuOpen] = useState(false);

  const ip        = status?.staIP || "—";
  const time      = status?.boardTime || "—";
  const stateStr  = (status?.stateLabel || status?.state || "IDLE").toUpperCase();
  const stateTone = STATE_TONES[stateStr] || "neutral";
  const mode      = MODE_INFO[streamMode] || MODE_INFO.connecting;
  const roleColor = ROLE_COLORS[authRole] || C.muted;
  const roleName  = (authRole || "operator").toUpperCase();

  return (
    <View style={styles.wrap}>
      {/* Row 1: title + state badge */}
      <View style={styles.row1}>
        <Text style={styles.title} numberOfLines={1}>LPG FILLING STATION</Text>
        <StatusChip label={stateStr} tone={stateTone} small />
      </View>

      {/* Row 2: connection info + role selector */}
      <View style={styles.row2}>
        <StatusChip label={mode.label} tone={mode.tone} small />
        <Text style={styles.ip} numberOfLines={1}>{ip}</Text>
        <Text style={styles.time} numberOfLines={1}>{time}</Text>

        {/* Role badge — tap to open role menu */}
        <Pressable style={[styles.roleBadge, { borderColor: roleColor }]} onPress={() => setMenuOpen(true)}>
          <Text style={[styles.roleText, { color: roleColor }]}>{roleName}</Text>
          <Text style={[styles.roleChevron, { color: roleColor }]}>▾</Text>
        </Pressable>
      </View>

      {/* Role menu modal */}
      <Modal transparent visible={menuOpen} animationType="fade" onRequestClose={() => setMenuOpen(false)}>
        <Pressable style={styles.overlay} onPress={() => setMenuOpen(false)}>
          <View style={styles.menu}>
            <Text style={styles.menuHeader}>Signed in as</Text>
            <Text style={styles.menuUser}>{authUsername || "—"}</Text>
            <View style={[styles.menuRoleRow, { borderColor: roleColor }]}>
              <Text style={[styles.menuRoleName, { color: roleColor }]}>{roleName}</Text>
            </View>
            <View style={styles.menuDivider} />
            <Pressable style={styles.menuItem} onPress={() => { setMenuOpen(false); onSwitchAccount?.(); }}>
              <Text style={styles.menuItemText}>Switch Account / Role</Text>
            </Pressable>
            <Pressable style={styles.menuItemClose} onPress={() => setMenuOpen(false)}>
              <Text style={styles.menuCloseText}>Close</Text>
            </Pressable>
          </View>
        </Pressable>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: C.surface,
    borderBottomWidth: 1,
    borderColor: C.border,
    paddingHorizontal: S.md,
    paddingTop: S.sm,
    paddingBottom: S.sm,
    gap: S.xs,
  },
  row1: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  row2: {
    flexDirection: "row",
    alignItems: "center",
    gap: S.sm,
  },
  title: {
    fontSize: T.md,
    fontWeight: "900",
    color: C.text,
    letterSpacing: 0.5,
    flex: 1,
  },
  ip: {
    fontSize: T.xs,
    fontWeight: "700",
    color: C.textSub,
    fontVariant: ["tabular-nums"],
    flex: 1,
  },
  time: {
    fontSize: T.xs,
    fontWeight: "700",
    color: C.textSub,
    fontVariant: ["tabular-nums"],
  },
  roleBadge: {
    flexDirection: "row",
    alignItems: "center",
    gap: 3,
    borderWidth: 1,
    borderRadius: R.pill,
    paddingHorizontal: S.sm,
    paddingVertical: 3,
  },
  roleText: {
    fontSize: T.xs,
    fontWeight: "900",
    letterSpacing: 0.5,
  },
  roleChevron: {
    fontSize: 9,
    fontWeight: "700",
  },

  // Modal
  overlay: {
    flex: 1,
    backgroundColor: C.bg + "99",
    justifyContent: "flex-start",
    alignItems: "flex-end",
    paddingTop: 72,
    paddingRight: S.md,
  },
  menu: {
    backgroundColor: C.surface,
    borderRadius: R.lg,
    borderWidth: 1,
    borderColor: C.border,
    minWidth: 200,
    padding: S.md,
    shadowColor: C.bg,
    shadowOpacity: 0.4,
    shadowRadius: 12,
    elevation: 8,
  },
  menuHeader: {
    fontSize: T.xs,
    fontWeight: "700",
    color: C.muted,
    textTransform: "uppercase",
    letterSpacing: 0.7,
    marginBottom: 2,
  },
  menuUser: {
    fontSize: T.md,
    fontWeight: "800",
    color: C.text,
    marginBottom: S.sm,
  },
  menuRoleRow: {
    borderWidth: 1,
    borderRadius: R.pill,
    alignSelf: "flex-start",
    paddingHorizontal: S.sm,
    paddingVertical: 3,
    marginBottom: S.sm,
  },
  menuRoleName: {
    fontSize: T.xs,
    fontWeight: "900",
    letterSpacing: 0.5,
  },
  menuDivider: {
    height: 1,
    backgroundColor: C.border,
    marginBottom: S.sm,
  },
  menuItem: {
    paddingVertical: S.sm,
  },
  menuItemText: {
    fontSize: T.sm,
    fontWeight: "700",
    color: C.active,
  },
  menuItemClose: {
    paddingVertical: S.sm,
    marginTop: S.xs,
  },
  menuCloseText: {
    fontSize: T.sm,
    fontWeight: "600",
    color: C.muted,
    textAlign: "center",
  },
});
