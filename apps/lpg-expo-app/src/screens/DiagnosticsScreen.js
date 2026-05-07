import { useEffect, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { fetchSystem, fetchStats } from "../services/controllerApi";
import { kg, money, duration } from "../utils/format";
import BottomNav from "../components/BottomNav";

const STAT_PERIODS = [
  { key: "today", label: "Today" },
  { key: "week",  label: "Week"  },
  { key: "month", label: "Month" },
  { key: "year",  label: "Year"  },
  { key: "all",   label: "All"   },
];

export default function DiagnosticsScreen() {
  const { navigate, activeUrl, authToken, status } = useAppState();
  const [sysInfo,   setSysInfo]   = useState(null);
  const [stats,     setStats]     = useState(null);
  const [period,    setPeriod]    = useState("today");
  const [expanded,  setExpanded]  = useState(false);

  useEffect(() => {
    let alive = true;
    const poll = async () => {
      try {
        if (alive) setSysInfo(await fetchSystem(activeUrl, authToken));
        if (alive) setStats(await fetchStats(activeUrl, authToken));
      } catch {}
    };
    poll();
    const t = setInterval(poll, 15000);
    return () => { alive = false; clearInterval(t); };
  }, [activeUrl, authToken]);

  const p = period;
  const completed = stats ? (p === "all" ? stats.allCompleted : stats[`${p}Completed`]) : "—";
  const failed    = stats ? (p === "all" ? stats.allFailed    : stats[`${p}Failed`])    : "—";
  const kgSold    = stats ? Number(p === "all" ? stats.allKg     : stats[`${p}Kg`])     : 0;
  const revenue   = stats ? Number(p === "all" ? stats.allAmount : stats[`${p}Amount`]) : 0;

  const uptimeSec = Math.round((status.uptimeMs || 0) / 1000);

  return (
    <View style={styles.shell}>
      <View style={styles.header}>
        <Pressable onPress={() => navigate("dashboard")} style={styles.backBtn}>
          <Text style={styles.backTxt}>← Back</Text>
        </Pressable>
        <Text style={styles.title}>Diagnostics</Text>
        <Pressable onPress={() => navigate("calibration")} style={styles.calBtn}>
          <Text style={styles.calTxt}>Calibration →</Text>
        </Pressable>
      </View>

      <ScrollView contentContainerStyle={styles.content}>

        {/* I/O Grid */}
        <SectionTitle>Relay Outputs</SectionTitle>
        <View style={styles.ioGrid}>
          {(status.relays || []).map((on, i) => (
            <IoCell key={i} label={`R${i + 1}`} active={on} />
          ))}
          {(!status.relays || status.relays.length === 0) && <Text style={styles.empty}>No relay data</Text>}
        </View>

        <SectionTitle style={{ marginTop: S.lg }}>Digital Inputs</SectionTitle>
        <View style={styles.ioGrid}>
          {(status.inputs || []).map((on, i) => (
            <IoCell key={i} label={`IN${i + 1}`} active={on} valueLabel={on ? "HI" : "LO"} />
          ))}
          {(!status.inputs || status.inputs.length === 0) && <Text style={styles.empty}>No input data</Text>}
        </View>

        {/* System resources */}
        {sysInfo && (
          <>
            <SectionTitle style={{ marginTop: S.lg }}>System Resources</SectionTitle>
            <GaugeRow label="Heap free" used={sysInfo.heapTotal - sysInfo.heapFree} total={sysInfo.heapTotal} unit="B" />
            <GaugeRow label="SPIFFS"    used={sysInfo.spiffsUsed} total={sysInfo.spiffsTotal} unit="B" />
            <InfoRow label="Uptime"       value={duration(uptimeSec)} />
            <InfoRow label="Boot reason"  value={status.bootReason || "—"} />
            <InfoRow label="Transactions" value={String(status.transactionCount || 0)} />
          </>
        )}

        {/* Transaction stats */}
        <SectionTitle style={{ marginTop: S.lg }}>Transaction Statistics</SectionTitle>
        <ScrollView horizontal showsHorizontalScrollIndicator={false} style={{ marginBottom: S.sm }}>
          <View style={{ flexDirection: "row", gap: S.xs }}>
            {STAT_PERIODS.map(({ key, label }) => (
              <Pressable key={key} style={[styles.chip, period === key && styles.chipOn]} onPress={() => setPeriod(key)}>
                <Text style={[styles.chipTxt, period === key && styles.chipTxtOn]}>{label}</Text>
              </Pressable>
            ))}
          </View>
        </ScrollView>
        <View style={styles.kpiRow}>
          {[
            { l: "COMPLETED",   v: String(completed)          },
            { l: "FAILED",      v: String(failed), warn: Number(failed) > 0 },
            { l: "KG SOLD",     v: `${kg(kgSold)} kg`         },
            { l: "REVENUE PKR", v: money(revenue)              },
          ].map(({ l, v, warn }) => (
            <View key={l} style={[styles.kpiCell, warn && styles.kpiWarn]}>
              <Text style={styles.kpiLabel}>{l}</Text>
              <Text style={[styles.kpiVal, warn && { color: C.danger }]}>{v}</Text>
            </View>
          ))}
        </View>

        {/* Status detail (collapsible) */}
        <Pressable style={styles.expandRow} onPress={() => setExpanded((p) => !p)}>
          <Text style={styles.expandLabel}>Status Detail</Text>
          <Text style={styles.expandIcon}>{expanded ? "▲" : "▼"}</Text>
        </Pressable>
        {expanded && <StatusDetailTable status={status} />}

      </ScrollView>
      <BottomNav active="diagnostics" />
    </View>
  );
}

function IoCell({ label, active, valueLabel }) {
  return (
    <View style={[styles.ioCell, active && styles.ioCellOn]}>
      <Text style={[styles.ioCellLabel, active && { color: C.ready }]}>{label}</Text>
      <Text style={[styles.ioCellVal,   active && { color: C.ready }]}>{valueLabel ?? (active ? "ON" : "—")}</Text>
    </View>
  );
}

function GaugeRow({ label, used, total, unit }) {
  if (!total) return null;
  const pct = Math.min(100, (used / total) * 100);
  const warn = pct > 80;
  const color = pct > 90 ? C.danger : pct > 70 ? C.warning : C.ready;
  return (
    <View style={{ marginBottom: S.md }}>
      <View style={{ flexDirection: "row", justifyContent: "space-between", marginBottom: S.xs }}>
        <Text style={styles.infoLabel}>{label}</Text>
        <Text style={styles.infoVal}>{Math.round(used / 1024)} / {Math.round(total / 1024)} K{unit}</Text>
      </View>
      <View style={styles.gaugeTrack}>
        <View style={[styles.gaugeFill, { width: `${pct}%`, backgroundColor: color }]} />
      </View>
    </View>
  );
}

function InfoRow({ label, value }) {
  return (
    <View style={styles.infoRow}>
      <Text style={styles.infoLabel}>{label}</Text>
      <Text style={styles.infoVal}>{value}</Text>
    </View>
  );
}

function StatusDetailTable({ status }) {
  const sections = [
    {
      title: "Fill Process",
      rows: [
        { l: "State",       v: status.stateLabel || status.state || "—" },
        { l: "Reason code", v: status.reasonCode || "—" },
      ],
    },
    {
      title: "Weight",
      rows: [
        { l: "Live",   v: `${kg(status.liveWeightKg ?? status.weightKg)} kg` },
        { l: "Tare",   v: `${kg(status.tareWeightKg)} kg` },
        { l: "Net",    v: `${kg(status.netWeightKg)} kg` },
        { l: "Target", v: `${kg(status.targetWeightKg)} kg` },
        { l: "Stable", v: status.weightStable ? "Yes" : "No", warn: !status.weightStable },
        { l: "Cal valid", v: status.calValid ? "Yes ✓" : "No ✗", warn: !status.calValid },
      ],
    },
    {
      title: "Safety Interlocks",
      rows: [
        { l: "E-Stop",   v: status.emergencyStopOk ? "OK" : "TRIPPED", warn: !status.emergencyStopOk },
        { l: "Cylinder", v: status.cylinderPresent  ? "Present" : "Absent" },
        { l: "Nozzle",   v: status.nozzleEngaged    ? "Engaged" : "Not engaged" },
        { l: "Sim mode", v: status.simActive         ? "ACTIVE ⚠" : "Off", warn: !!status.simActive },
      ],
    },
    {
      title: "Network",
      rows: [
        { l: "WiFi",       v: status.wifiConnected ? "Connected" : "Disconnected", warn: !status.wifiConnected },
        { l: "STA IP",     v: status.staIP     || "—" },
        { l: "RSSI",       v: status.wifiRssi  ? `${status.wifiRssi} dBm` : "—", warn: status.wifiRssi < -80 },
        { l: "Board time", v: status.boardTime || "—" },
      ],
    },
  ];

  return (
    <View style={styles.detailTable}>
      {sections.map(({ title, rows }) => (
        <View key={title} style={{ marginBottom: S.md }}>
          <Text style={styles.detailSection}>{title}</Text>
          {rows.map(({ l, v, warn }) => (
            <View key={l} style={styles.detailRow}>
              <Text style={styles.detailLabel}>{l}</Text>
              <Text style={[styles.detailVal, warn && { color: C.warning }]}>{v}</Text>
            </View>
          ))}
        </View>
      ))}
    </View>
  );
}

function SectionTitle({ children, style }) {
  return <Text style={[styles.sectionTitle, style]}>{children}</Text>;
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  header:  { flexDirection: "row", alignItems: "center", paddingHorizontal: S.md, paddingVertical: S.sm + 2, backgroundColor: C.surface, borderBottomWidth: 1, borderBottomColor: C.border, gap: S.md },
  backBtn: { padding: S.xs },
  backTxt: { color: C.primary, fontSize: T.md, fontWeight: "700" },
  title:   { flex: 1, color: C.text, fontSize: T.md, fontWeight: "900" },
  calBtn:  { padding: S.xs },
  calTxt:  { color: C.primary, fontSize: T.sm, fontWeight: "700" },
  content: { padding: S.md, paddingBottom: S.xl },

  sectionTitle: { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.sm },
  empty:        { color: C.muted, fontSize: T.sm },

  ioGrid:    { flexDirection: "row", flexWrap: "wrap", gap: S.sm },
  ioCell:    { width: 60, height: 60, borderRadius: R.md, alignItems: "center", justifyContent: "center", backgroundColor: C.surface, borderWidth: 1, borderColor: C.border },
  ioCellOn:  { backgroundColor: C.ready + "1f", borderColor: C.ready },
  ioCellLabel:{ fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase" },
  ioCellVal:  { fontSize: T.sm, fontWeight: "900", color: C.muted, marginTop: 2 },

  infoRow:   { flexDirection: "row", justifyContent: "space-between", paddingVertical: S.xs + 1, borderBottomWidth: 1, borderBottomColor: C.border },
  infoLabel: { fontSize: T.sm, color: C.textSub },
  infoVal:   { fontSize: T.sm, fontWeight: "700", color: C.text },

  gaugeTrack: { height: 8, backgroundColor: C.surface2, borderRadius: R.pill, overflow: "hidden" },
  gaugeFill:  { height: 8, borderRadius: R.pill },

  chip:       { paddingVertical: S.xs + 1, paddingHorizontal: S.md, borderRadius: R.pill, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border },
  chipOn:     { backgroundColor: C.primary, borderColor: C.primary },
  chipTxt:    { fontSize: T.xs + 1, fontWeight: "700", color: C.textSub },
  chipTxtOn:  { color: C.white },

  kpiRow:    { flexDirection: "row", flexWrap: "wrap", gap: S.sm, marginBottom: S.lg },
  kpiCell:   { flex: 1, minWidth: "45%", backgroundColor: C.surface, borderRadius: R.md, borderWidth: 1, borderColor: C.border, padding: S.sm },
  kpiWarn:   { backgroundColor: C.danger + "1a", borderColor: C.danger },
  kpiLabel:  { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7 },
  kpiVal:    { fontSize: T.xl, fontWeight: "900", color: C.text, marginTop: S.xs, fontVariant: ["tabular-nums"] },

  expandRow:   { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: S.md, borderTopWidth: 1, borderTopColor: C.border, marginTop: S.sm },
  expandLabel: { color: C.text, fontSize: T.md, fontWeight: "800" },
  expandIcon:  { color: C.textSub, fontSize: T.sm },

  detailTable:   { backgroundColor: C.surface, borderRadius: R.md, borderWidth: 1, borderColor: C.border, padding: S.md, marginBottom: S.md },
  detailSection: { fontSize: T.xs, fontWeight: "800", color: C.primary, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.xs, marginTop: S.xs },
  detailRow:     { flexDirection: "row", justifyContent: "space-between", paddingVertical: 3, borderBottomWidth: 1, borderBottomColor: C.border },
  detailLabel:   { fontSize: T.xs, color: C.textSub, flex: 1 },
  detailVal:     { fontSize: T.xs, fontWeight: "700", color: C.text, textAlign: "right", flex: 1, fontVariant: ["tabular-nums"] },
});
