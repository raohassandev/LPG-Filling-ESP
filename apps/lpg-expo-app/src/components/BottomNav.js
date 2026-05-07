import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { useAppState } from "../state/AppStateProvider";
import { C, S, T } from "../theme";

const NAV = {
  operator: [
    ["dashboard", "Dashboard"],
    ["transactions", "History"],
    ["__logout", "Sign Out"],
  ],
  admin: [
    ["dashboard", "Dashboard"],
    ["transactions", "History"],
    ["network", "Settings"],
    ["admin-users", "Users"],
    ["__logout", "Sign Out"],
  ],
  manufacturer: [
    ["dashboard", "Dashboard"],
    ["transactions", "History"],
    ["network", "Settings"],
    ["admin-users", "Users"],
    ["diagnostics", "Diagnostics"],
    ["calibration", "Calibration"],
    ["__logout", "Sign Out"],
  ],
};

export default function BottomNav({ active }) {
  const { authRole, navigate, logout } = useAppState();
  const items = NAV[authRole] || NAV.operator;

  return (
    <View style={styles.bar}>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.row}>
        {items.map(([key, label]) => {
          const isActive = key === active;
          const onPress = key === "__logout" ? logout : () => navigate(key);
          return (
            <Pressable key={key} style={[styles.item, isActive && styles.itemActive]} onPress={onPress}>
              <Text style={[styles.label, isActive && styles.labelActive]}>{label}</Text>
            </Pressable>
          );
        })}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  bar:         { height: 52, backgroundColor: C.surface, borderTopWidth: 1, borderTopColor: C.border },
  row:         { flexDirection: "row", alignItems: "stretch" },
  item:        { paddingHorizontal: S.lg, justifyContent: "center", borderTopWidth: 2, borderTopColor: "transparent" },
  itemActive:  { borderTopColor: C.primary },
  label:       { fontSize: T.xs, fontWeight: "800", color: C.muted, textTransform: "uppercase", letterSpacing: 0.7 },
  labelActive: { color: C.primary },
});
