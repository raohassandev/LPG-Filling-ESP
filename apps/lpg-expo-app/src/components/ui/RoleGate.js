// Renders children only if the current authRole is in the allowed list.
// Roles in this firmware: "operator" | "admin" | "manufacturer".

import { useAppState } from "../../state/AppStateProvider";

const RANK = { operator: 1, admin: 2, manufacturer: 3 };

export default function RoleGate({ allow = [], minRole, fallback = null, children }) {
  const { authRole } = useAppState();

  if (minRole) {
    const have = RANK[authRole] || 0;
    const need = RANK[minRole] || 0;
    if (have < need) return fallback;
    return children;
  }

  if (Array.isArray(allow) && allow.length > 0) {
    if (!allow.includes(authRole)) return fallback;
  }
  return children;
}
