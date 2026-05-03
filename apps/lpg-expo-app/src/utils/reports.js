export function inPeriod(item, period) {
  if (period === "all") return true;
  const seconds = Number(item.endTime || item.startTime || 0);
  if (!seconds) return true;

  const date = new Date(seconds * 1000);
  const now = new Date();

  if (period === "today") return date.toDateString() === now.toDateString();
  if (period === "week") {
    const start = new Date(now);
    start.setHours(0, 0, 0, 0);
    start.setDate(now.getDate() - now.getDay());
    return date >= start;
  }
  if (period === "month") {
    return date.getFullYear() === now.getFullYear() && date.getMonth() === now.getMonth();
  }
  return date.getFullYear() === now.getFullYear();
}

export function summarizeTransactions(transactions, period) {
  const filtered = transactions.filter((item) => inPeriod(item, period));
  const completedRows = filtered.filter((item) => Number(item.status) === 1);
  const exceptionRows = filtered.filter((item) => Number(item.status) === 2 || Number(item.status) === 3);
  const totalSales = completedRows.reduce((sum, item) => sum + Number(item.finalAmount || 0), 0);
  const totalKg = completedRows.reduce((sum, item) => sum + Number(item.netKg || 0), 0);

  return {
    filtered,
    completed: completedRows.length,
    exceptions: exceptionRows.length,
    totalSales,
    totalKg,
    averageSale: completedRows.length ? totalSales / completedRows.length : 0,
  };
}
