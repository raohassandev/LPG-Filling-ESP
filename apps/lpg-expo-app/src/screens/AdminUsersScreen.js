import { useEffect, useState } from "react";
import { Alert, Pressable, ScrollView, StyleSheet, Text, TextInput, View } from "react-native";
import { C, R, S, T } from "../theme";
import { useAppState } from "../state/AppStateProvider";
import { fetchUsers, createUser, updateUser, deleteUser } from "../services/controllerApi";
import { Field, Input } from "../components/ui/Field";
import SegmentedControl from "../components/ui/SegmentedControl";
import Button from "../components/ui/Button";
import StatusChip from "../components/ui/StatusChip";

const ROLES = [
  { key: "operator",     label: "Operator"     },
  { key: "admin",        label: "Admin"         },
  { key: "manufacturer", label: "Manufacturer"  },
];

export default function AdminUsersScreen() {
  const { navigate, activeUrl, authToken } = useAppState();
  const [users,     setUsers]     = useState([]);
  const [loading,   setLoading]   = useState(true);
  const [showCreate,setShowCreate]= useState(false);
  const [editUser,  setEditUser]  = useState(null);
  const [result,    setResult]    = useState(null);

  async function load() {
    setLoading(true);
    try { setUsers(await fetchUsers(activeUrl, authToken)); }
    catch (err) { setResult({ ok: false, msg: err?.message || "Failed to load users" }); }
    finally { setLoading(false); }
  }

  useEffect(() => { load(); }, [activeUrl, authToken]);

  async function handleDelete(username) {
    Alert.alert("Delete user", `Remove "${username}"? This cannot be undone.`, [
      { text: "Cancel", style: "cancel" },
      {
        text: "Delete", style: "destructive",
        onPress: async () => {
          try {
            await deleteUser(activeUrl, username, authToken);
            setResult({ ok: true, msg: `${username} deleted` });
            await load();
          } catch (err) {
            setResult({ ok: false, msg: err?.message || "Delete failed" });
          }
        },
      },
    ]);
  }

  return (
    <View style={styles.shell}>
      <View style={styles.header}>
        <Pressable onPress={() => navigate("dashboard")} style={styles.backBtn}>
          <Text style={styles.backTxt}>← Back</Text>
        </Pressable>
        <Text style={styles.title}>Users</Text>
        <Pressable onPress={() => { setShowCreate(true); setEditUser(null); }} style={styles.addBtn}>
          <Text style={styles.addTxt}>+ Add</Text>
        </Pressable>
      </View>

      <ScrollView contentContainerStyle={styles.content}>
        {result && (
          <Text style={[styles.resultMsg, { color: result.ok ? C.ready : C.danger }]}>{result.msg}</Text>
        )}

        {showCreate && !editUser && (
          <UserForm
            title="New User"
            onSave={async (fields) => {
              await createUser(activeUrl, fields.username, fields.password, fields.role, fields.canSetRate === "1", authToken);
              setShowCreate(false);
              setResult({ ok: true, msg: `${fields.username} created` });
              await load();
            }}
            onCancel={() => setShowCreate(false)}
          />
        )}

        {editUser && (
          <UserForm
            title={`Edit: ${editUser.username}`}
            initial={editUser}
            onSave={async (fields) => {
              await updateUser(activeUrl, editUser.username, {
                role: fields.role,
                canSetRate: fields.canSetRate === "1",
                ...(fields.password ? { password: fields.password } : {}),
              }, authToken);
              setEditUser(null);
              setResult({ ok: true, msg: `${editUser.username} updated` });
              await load();
            }}
            onCancel={() => setEditUser(null)}
          />
        )}

        {loading ? (
          <Text style={styles.empty}>Loading users…</Text>
        ) : users.length === 0 ? (
          <Text style={styles.empty}>No users found.</Text>
        ) : (
          users.map((u) => (
            <View key={u.username} style={styles.userRow}>
              <View style={{ flex: 1 }}>
                <View style={{ flexDirection: "row", alignItems: "center", gap: S.sm }}>
                  <Text style={styles.username}>{u.username}</Text>
                  <StatusChip label={u.role} tone={u.role === "manufacturer" ? "warning" : u.role === "admin" ? "active" : "ready"} />
                  {u.canSetRate && <StatusChip label="rate" tone="ready" />}
                </View>
                {u.lastLogin && <Text style={styles.lastLogin}>Last login: {u.lastLogin}</Text>}
              </View>
              <View style={{ flexDirection: "row", gap: S.xs }}>
                <Pressable style={styles.rowBtn} onPress={() => { setEditUser(u); setShowCreate(false); }}>
                  <Text style={styles.rowBtnTxt}>Edit</Text>
                </Pressable>
                <Pressable style={[styles.rowBtn, styles.rowBtnDanger]} onPress={() => handleDelete(u.username)}>
                  <Text style={[styles.rowBtnTxt, { color: C.danger }]}>Delete</Text>
                </Pressable>
              </View>
            </View>
          ))
        )}
      </ScrollView>
    </View>
  );
}

function UserForm({ title, initial, onSave, onCancel }) {
  const [username,   setUsername]   = useState(initial?.username || "");
  const [password,   setPassword]   = useState("");
  const [role,       setRole]       = useState(initial?.role || "operator");
  const [canSetRate, setCanSetRate] = useState(initial?.canSetRate ? "1" : "0");
  const [busy,       setBusy]       = useState(false);
  const [err,        setErr]        = useState("");

  async function submit() {
    setErr("");
    if (!initial && !username.trim()) { setErr("Username required"); return; }
    if (!initial && !password.trim()) { setErr("Password required"); return; }
    setBusy(true);
    try {
      await onSave({ username: username.trim(), password, role, canSetRate });
    } catch (e) {
      setErr(e?.message || "Failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <View style={styles.form}>
      <Text style={styles.formTitle}>{title}</Text>
      {!initial && (
        <Field label="Username">
          <Input value={username} onChangeText={setUsername} autoCapitalize="none" autoCorrect={false} />
        </Field>
      )}
      <Field label={initial ? "New password (leave blank to keep)" : "Password"}>
        <Input value={password} onChangeText={setPassword} secureTextEntry autoCapitalize="none" />
      </Field>
      <Field label="Role">
        <SegmentedControl options={ROLES} value={role} onChange={setRole} style={{ marginTop: S.xs }} />
      </Field>
      <Field label="Can set fill rate">
        <SegmentedControl
          options={[{ key: "0", label: "No" }, { key: "1", label: "Yes" }]}
          value={canSetRate}
          onChange={setCanSetRate}
          style={{ marginTop: S.xs }}
        />
      </Field>
      {!!err && <Text style={styles.err}>{err}</Text>}
      <View style={{ flexDirection: "row", gap: S.sm, marginTop: S.md }}>
        <Button label="Cancel" variant="secondary" size="md" onPress={onCancel} style={{ flex: 1 }} />
        <Button label={busy ? "Saving…" : "Save"} variant="primary" size="md" onPress={submit} loading={busy} style={{ flex: 1 }} />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  shell:   { flex: 1, backgroundColor: C.bg },
  header:  { flexDirection: "row", alignItems: "center", paddingHorizontal: S.md, paddingVertical: S.sm + 2, backgroundColor: C.surface, borderBottomWidth: 1, borderBottomColor: C.border, gap: S.md },
  backBtn: { padding: S.xs },
  backTxt: { color: C.primary, fontSize: T.md, fontWeight: "700" },
  title:   { flex: 1, color: C.text, fontSize: T.md, fontWeight: "900" },
  addBtn:  { paddingVertical: S.xs, paddingHorizontal: S.sm },
  addTxt:  { color: C.primary, fontSize: T.md, fontWeight: "700" },
  content: { padding: S.md, paddingBottom: 40 },

  resultMsg: { fontSize: T.sm, textAlign: "center", marginBottom: S.md },
  empty:     { color: C.muted, fontSize: T.sm, textAlign: "center", paddingVertical: S.xl },

  userRow:    { flexDirection: "row", alignItems: "center", paddingVertical: S.md, borderTopWidth: 1, borderTopColor: C.border },
  username:   { fontSize: T.md, fontWeight: "800", color: C.text },
  lastLogin:  { fontSize: T.xs, color: C.muted, marginTop: 2 },
  rowBtn:     { paddingVertical: S.xs + 1, paddingHorizontal: S.sm, borderRadius: R.sm, borderWidth: 1, borderColor: C.border },
  rowBtnDanger: { borderColor: C.danger + "66" },
  rowBtnTxt:  { fontSize: T.xs + 1, fontWeight: "700", color: C.textSub },

  form:      { backgroundColor: C.surface, borderRadius: R.lg, borderWidth: 1, borderColor: C.border, padding: S.md, marginBottom: S.md },
  formTitle: { fontSize: T.md, fontWeight: "900", color: C.text, marginBottom: S.sm },
  err:       { color: C.danger, fontSize: T.xs, marginTop: S.sm },
});
