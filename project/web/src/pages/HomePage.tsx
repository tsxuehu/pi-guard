import { Card, List } from 'antd-mobile'

export default function HomePage() {
  return (
    <div className="page">
      <Card title="实时监控">
        <List>
          <List.Item description="RTMP / HLS 播放入口">视频流</List.Item>
          <List.Item description="移动侦测事件会在此展示">告警事件</List.Item>
        </List>
      </Card>
    </div>
  )
}
