import { NavBar, TabBar } from 'antd-mobile'
import { AppOutline, InformationCircleOutline } from 'antd-mobile-icons'
import { Navigate, Route, Routes, useLocation, useNavigate } from 'react-router-dom'
import AboutPage from './pages/AboutPage'
import HomePage from './pages/HomePage'

function AppShell() {
  const location = useLocation()
  const navigate = useNavigate()

  const tabs = [
    { key: '/home', title: '首页', icon: <AppOutline /> },
    { key: '/about', title: '关于', icon: <InformationCircleOutline /> },
  ]

  return (
    <div className="app-shell">
      <NavBar back={null}>pi-guard-web</NavBar>
      <main className="app-content">
        <Routes>
          <Route path="/home" element={<HomePage />} />
          <Route path="/about" element={<AboutPage />} />
          <Route path="*" element={<Navigate to="/home" replace />} />
        </Routes>
      </main>
      <TabBar activeKey={location.pathname} onChange={(key) => navigate(key)}>
        {tabs.map((item) => (
          <TabBar.Item key={item.key} icon={item.icon} title={item.title} />
        ))}
      </TabBar>
    </div>
  )
}

export default function App() {
  return <AppShell />
}
