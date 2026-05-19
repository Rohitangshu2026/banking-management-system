import { Route, Routes, Navigate } from "react-router-dom";
import Login from "./pages/Login";
import CustomerDashboard from "./pages/CustomerDashboard";
import EmployeeConsole from "./pages/EmployeeConsole";
import ManagerConsole from "./pages/ManagerConsole";
import AdminConsole from "./pages/AdminConsole";
import { AuthProvider, useAuth } from "./auth";
import { ToastProvider } from "./components/Toast";

function RequireRole({
  role,
  children,
}: {
  role: "customer" | "employee" | "manager" | "admin";
  children: React.ReactNode;
}) {
  const { me } = useAuth();
  if (!me) return <Navigate to="/login" replace />;
  if (me.role !== role) return <Navigate to={`/${me.role}`} replace />;
  return <>{children}</>;
}

export default function App() {
  return (
    <ToastProvider>
    <AuthProvider>
      <Routes>
        <Route path="/" element={<Navigate to="/login" replace />} />
        <Route path="/login" element={<Login />} />
        <Route
          path="/customer"
          element={
            <RequireRole role="customer">
              <CustomerDashboard />
            </RequireRole>
          }
        />
        <Route
          path="/employee"
          element={
            <RequireRole role="employee">
              <EmployeeConsole />
            </RequireRole>
          }
        />
        <Route
          path="/manager"
          element={
            <RequireRole role="manager">
              <ManagerConsole />
            </RequireRole>
          }
        />
        <Route
          path="/admin"
          element={
            <RequireRole role="admin">
              <AdminConsole />
            </RequireRole>
          }
        />
        <Route path="*" element={<Navigate to="/login" replace />} />
      </Routes>
    </AuthProvider>
    </ToastProvider>
  );
}
