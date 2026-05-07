import { useEffect, useRef } from "react";
import { Animated, StyleSheet, View } from "react-native";
import { MaterialCommunityIcons } from "@expo/vector-icons";
import { C, R, S } from "../theme";

// Four readiness icons in a horizontal strip — color only, no text labels.
// Icon meanings should be universally recognisable on the factory floor.
//
// ⬡ alert-octagon  — Emergency stop (octagonal stop sign shape)
// ⬤ gas-cylinder   — Cylinder present (literal gas bottle)
// ⬤ fuel           — Nozzle engaged (filling nozzle / dispenser)
// ⚖ scale-balance  — Weight stable (weighing scale)

const INDICATORS = [
  {
    key: "estop",
    icon: "alert-octagon",
    activeKey: "emergencyStopOk",
    activeColor: C.ready,
    inactiveColor: C.danger,   // e-stop OFF = danger (bad)
    invertLogic: true,         // icon active when flag is TRUE (safe = green)
  },
  {
    key: "cylinder",
    icon: "gas-cylinder",
    activeKey: "cylinderPresent",
    activeColor: C.ready,
    inactiveColor: C.muted,
  },
  {
    key: "nozzle",
    icon: "fuel",
    activeKey: "nozzleEngaged",
    activeColor: C.active,
    inactiveColor: C.muted,
  },
  {
    key: "weight",
    icon: "scale-balance",
    activeKey: "weightStable",
    activeColor: C.ready,
    inactiveColor: C.warning,  // unstable = amber (measuring)
    blinkWhenInactive: true,
  },
];

export default function ReadinessCard({ status }) {
  return (
    <View style={styles.strip}>
      {INDICATORS.map((ind) => (
        <ReadinessIcon key={ind.key} indicator={ind} status={status} />
      ))}
    </View>
  );
}

function ReadinessIcon({ indicator, status }) {
  const { icon, activeKey, activeColor, inactiveColor, blinkWhenInactive } = indicator;
  const isActive = !!status?.[activeKey];
  const color    = isActive ? activeColor : inactiveColor;

  // Blink animation for weight-stable when measuring
  const blinkAnim = useRef(new Animated.Value(1)).current;
  useEffect(() => {
    if (blinkWhenInactive && !isActive) {
      const anim = Animated.loop(
        Animated.sequence([
          Animated.timing(blinkAnim, { toValue: 0.2, duration: 500, useNativeDriver: true }),
          Animated.timing(blinkAnim, { toValue: 1,   duration: 500, useNativeDriver: true }),
        ])
      );
      anim.start();
      return () => anim.stop();
    } else {
      blinkAnim.setValue(1);
    }
  }, [isActive, blinkWhenInactive]);

  const iconEl = (
    <View style={[styles.iconWrap, { borderColor: color + "44", backgroundColor: color + "18" }]}>
      <MaterialCommunityIcons name={icon} size={32} color={color} />
    </View>
  );

  if (blinkWhenInactive && !isActive) {
    return <Animated.View style={{ opacity: blinkAnim }}>{iconEl}</Animated.View>;
  }
  return iconEl;
}

const styles = StyleSheet.create({
  strip: {
    flexDirection: "row",
    gap: S.sm,
    flex: 1,
  },
  iconWrap: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: S.md,
    borderRadius: R.md,
    borderWidth: 1,
  },
});
