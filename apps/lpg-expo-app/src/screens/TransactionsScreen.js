import { useEffect, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { fetchSdMonths, fetchSdTransactions } from "../services/controllerApi";
import { money, kg, shortDateTime } from "../utils/format";
import BottomNav from "../components/BottomNav";

const PERIODS = [
  { key: "today", label: "Today" },
  { key: "week",  label: "Week"  },
  { key: "month", label: "Month" },
  { key: "year",  label: "Year"  },
  { key: "all",   label: "All"   },
];

export default function TransactionsScreen() {
  const { navigate, transactions, activeUrl, authToken, authRole } = useAppState();
  const [period,    setPeriod]    = useState("all");
  const [sdMonths,  setSdMonths]  = useState([]);
  const [sdReady,   setSdReady]   = useState(false);
  const [selMonth,  setSelMonth]  = useState(null);
  const [sdRecords, setSdRecords] = useState([]);
  const [sdLoading, setSdLoading] = useState(false);

  const isAdmin = authRole === "admin" || authRole === "manufacturer";

  useEffect(() => {
    if (!isAdmin) return;
    fetchSdMonths(activeUrl, authToken)
      .then((d) => { setSdReady(!!d.ready); setSdMonths(Array.isArray(d.months) ? d.months : []); })
      .catch(() => {});
  }, [activeUrl, authToken, isAdmin]);

  async function loadSdMonth(month) {
    setSelMonth(month); setSdLoading(true); setSdRecords([]);
    try { setSdRecords(await fetchSdTransactions(activeUrl, month, authToken)); }
    catch {}
    setSdLoading(false);
  }

  const filtered = transactions.filter((item) => {
    if (period === "all") return true;
    const sec = Number(item.endTime || item.startTime || 0);
    if (!sec) return true;
    const d = new Date(sec * 1000), now = new Date();
    if (period === "today") return d.toDateString() === now.toDateString();
    if (period === "week")  { const s = new Date(now); s.setHours(0,0,0,0); s.setDate(now.getDate()-now.getDay()); return d >= s; }
    if (period === "month") return d.getFullYear() === now.getFullYear() && d.getMonth() === now.getMonth();
    return d.getFullYear() === now.getFullYear();
  });

  const done  = filtered.filter((i) => Number(i.status) === 1);
  const excp  = filtered.filter((i) => Number(i.status) >= 2);
  const sales = done.reduce((s, i) => s + Number(i.finalAmount || 0), 0);
  const kgSold= done.reduce((s, i) => s + Number(i.netKg || 0), 0);

  return (
    <View style={styles.shell}>
      {/* Header */}
      <View style={styles.header}>
        <Pressable onPress={() => navigate("dashboard")} style={styles.backBtn}>
          <Text style={styles.backTxt}>← Back</Text>
        </Pressable>
        <Text style={styles.title}>{authRole === "operator" ? "My Transactions" : "All Transactions"}</Text>
      </View>

      <ScrollView contentContainerStyle={styles.content}>
        {/* Period tabs */}
        <ScrollView horizontal showsHorizontalScrollIndicator={false} style={{ marginBottom: S.md }}>
          <View style={styles.chips}>
            {PERIODS.map(({ key, label }) => (
              <Pressable key={key} style={[styles.chip, period === key && styles.chipOn]} onPress={() => setPeriod(key)}>
                <Text style={[styles.chipTxt, period === key && styles.chipTxtOn]}>{label}</Text>
              </Pressable>
            ))}
          </View>
        </ScrollView>

        {/* KPI row */}
        <View style={styles.kpiRow}>
          {[
            { l: "COMPLETED",   v: String(done.length)         },
            { l: "EXCEPTIONS",  v: String(excp.length), warn: excp.length > 0 },
            { l: "KG SOLD",     v: `${kg(kgSold)} kg`          },
            { l: "REVENUE PKR", v: money(sales)                },
          ].map(({ l, v, warn }) => (
            <View key={l} style={[styles.kpiCell, warn && styles.kpiWarn]}>
              <Text style={styles.kpiLabel}>{l}</Text>
              <Text style={[styles.kpiVal, warn && { color: C.danger }]}>{v}</Text>
            </View>
          ))}
        </View>

        {/* Transaction list */}
        <Text style={styles.sectionTitle}>Recent Transactions</Text>
        {filtered.length === 0 ? (
          <Text style={styles.empty}>No transactions in this period.</Text>
        ) : (
          filtered.slice(-50).reverse().map((item) => <TxnRow key={item.transactionId || item.id} item={item} />)
        )}

        {/* SD archive (admin+) */}
        {isAdmin && sdReady && (
          <>
            <Text style={[styles.sectionTitle, { marginTop: S.xl }]}>SD Archive</Text>
            <ScrollView horizontal showsHorizontalScrollIndicator={false} style={{ marginBottom: S.sm }}>
              <View style={styles.chips}>
                {sdMonths.map((m) => (
                  <Pressable key={m} style={[styles.chip, selMonth === m && styles.chipOn]} onPress={() => loadSdMonth(m)}>
                    <Text style={[styles.chipTxt, selMonth === m && styles.chipTxtOn]}>{m}</Text>
                  </Pressable>
                ))}
              </View>
            </ScrollView>
            {sdLoading && <Text style={styles.empty}>Loading…</Text>}
            {sdRecords.map((rec, i) => <TxnRow key={rec.id ?? i} item={rec} />)}
            {!sdLoading && selMonth && sdRecords.length === 0 && (
              <Text style={styles.empty}>No records for {selMonth}</Text>
            )}
          </>
        )}
      </ScrollView>
      <BottomNav active="transactions" />
    </View>
  );
}

function TxnRow({ item }) {
  const isOk  = Number(item.status) === 1;
  const dateStr = shortDateTime(item.endTime || item.startTime);
  return (
    <View style={styles.txnRow}>
      <View style={[styles.dot, { backgroundColor: isOk ? C.ready : C.warning }]} />
      <Text style={styles.txnId} numberOfLines={1}>{item.transactionId || `#${item.id}`}</Text>
      <Text style={styles.txnKg} numberOfLines={1}>{kg(item.netKg ?? item.finalKg)} kg</Text>
      <Text style={styles.txnAmt} numberOfLines={1}>PKR {money(item.finalAmount)}</Text>
      <Text style={styles.txnTime} numberOfLines={1}>{dateStr || "-"}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  header:  { flexDirection: "row", alignItems: "center", paddingHorizontal: S.md, paddingVertical: S.sm + 2, backgroundColor: C.surface, borderBottomWidth: 1, borderBottomColor: C.border, gap: S.md },
  backBtn: { padding: S.xs },
  backTxt: { color: C.primary, fontSize: T.md, fontWeight: "700" },
  title:   { color: C.text, fontSize: T.md, fontWeight: "900" },
  content: { padding: S.md, paddingBottom: S.xl },

  chips:      { flexDirection: "row", gap: S.xs },
  chip:       { paddingVertical: S.xs + 2, paddingHorizontal: S.md, borderRadius: R.pill, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border },
  chipOn:     { backgroundColor: C.primary, borderColor: C.primary },
  chipTxt:    { fontSize: T.sm, fontWeight: "700", color: C.textSub },
  chipTxtOn:  { color: C.white },

  kpiRow:    { flexDirection: "row", flexWrap: "wrap", gap: S.sm, marginBottom: S.lg },
  kpiCell:   { flex: 1, minWidth: "45%", backgroundColor: C.surface, borderRadius: R.md, borderWidth: 1, borderColor: C.border, padding: S.sm },
  kpiWarn:   { backgroundColor: C.danger + "1a", borderColor: C.danger },
  kpiLabel:  { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7 },
  kpiVal:    { fontSize: T.xl, fontWeight: "900", color: C.text, marginTop: S.xs, fontVariant: ["tabular-nums"] },

  sectionTitle: { fontSize: T.xs, fontWeight: "800", color: C.textSub, textTransform: "uppercase", letterSpacing: 0.7, marginBottom: S.sm },
  empty:        { color: C.muted, fontSize: T.sm, textAlign: "center", paddingVertical: S.lg },

  txnRow:  { flexDirection: "row", alignItems: "center", gap: S.sm, paddingVertical: S.sm, borderTopWidth: 1, borderTopColor: C.border },
  dot:     { width: 8, height: 8, borderRadius: 4 },
  txnId:   { flex: 1.3, fontSize: T.sm, fontWeight: "900", color: C.text },
  txnKg:   { flex: 0.9, fontSize: T.xs, color: C.textSub, fontWeight: "700", fontVariant: ["tabular-nums"] },
  txnAmt:  { flex: 1.2, fontSize: T.xs, color: C.active, fontWeight: "800", fontVariant: ["tabular-nums"] },
  txnTime: { flex: 1.4, fontSize: T.xs, color: C.muted, textAlign: "right" },
});
