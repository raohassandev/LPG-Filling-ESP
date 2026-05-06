import { useState } from "react";
import { Pressable, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import StatusChip from "./ui/StatusChip";

const MODE_INFO = {
  websocket:  { label: "WS",   tone: "ready" },
  mqtt:       { label: "MQTT", tone: "active" },
  polling:    { label: "REST", tone: "warning" },
  offline:    { label: "OFF",  tone: "danger" },
  connecting: { label: "...",  tone: "neutral" },
};

export default function StatusHeader({ status, streamMode, firmware, onTapTitle }) {
  const [open, setOpen] = useState(false);

  const ip   = status?.staIP || "—";
  const time = status?.boardTime || "—";
  const wifi = !!status?.wifiConnected;
  const rssi = Number(status?.wifiRssi || 0);
  const mode = MODE_INFO[streamMode] || MODE_INFO.connecting;

  return (
    <View style={styles.wrap}>
      <Pressable onPress={() => setOpen(o => !o)} style={styles.row}>
        <StatusChip label={mode.label} tone={mode.tone} small />
        <Text style={styles.ip}>{ip}</Text>
        <Text style={[styles.wifi, { color: wifi ? C.ready : C.warning }]} numberOfLines={1}>
          {wifi ? `WiFi ${rssi || "—"}dBm` : "No WiFi"}
        </Text>
        <Text style={styles.time}>{time}</Text>
        <Text style={styles.expand}>{open ? "▲" : "▼"}</Text>
      </Pressable>

      {open && (
        <View style={styles.detail}>
          <Detail label="Firmware" value={firmware || status?.firmware || "—"} />
          <Detail label="State" value={status?.stateLabel || status?.state || "—"} />
          <Detail label="Stream" value={streamMode || "—"} />
          <Detail label="RSSI" value={rssi ? `${rssi} dBm` : "—"} />
        </View>
      )}
    </View>
  );
}

function Detail({ label, value }) {
  return (
    <View style={styles.detailRow}>
      <Text style={styles.detailLabel}>{label}</Text>
      <Text style={styles.detailVal} numberOfLines={1}>{value}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: C.surface,
    borderBottomWidth: 1,
    borderColor: C.border,
  },
  row: {
    flexDirection: "row",
    alignItems: "center",
    gap: S.sm,
    paddingHorizontal: S.md,
    paddingVertical: S.sm,
  },
  ip:    { color: C.text, fontSize: T.sm, fontWeight: "700", fontVariant: ["tabular-nums"], marginLeft: S.xs },
  wifi:  { fontSize: T.xs, fontWeight: "700", flex: 1, textAlign: "right" },
  time:  { color: C.textSub, fontSize: T.xs, fontVariant: ["tabular-nums"] },
  expand:{ color: C.textSub, fontSize: T.xs, marginLeft: S.xs },
  detail:{ paddingHorizontal: S.md, paddingBottom: S.sm },
  detailRow: { flexDirection: "row", justifyContent: "space-between", paddingVertical: 2 },
  detailLabel: { fontSize: T.xs, color: C.textSub, fontWeight: "700", textTransform: "uppercase", letterSpacing: 0.6 },
  detailVal: { fontSize: T.xs, color: C.text, fontWeight: "600", fontVariant: ["tabular-nums"] },
});
