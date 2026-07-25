import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import StatusBadge from '../../src/components/StatusBadge.vue'

describe('StatusBadge', () => {
  it('renders default status (loading)', () => {
    const wrapper = mount(StatusBadge)

    expect(wrapper.classes()).toContain('status-badge--loading')
    expect(wrapper.text()).toContain('...')
  })

  it('renders ok status', () => {
    const wrapper = mount(StatusBadge, {
      props: {
        status: 'ok',
      },
    })

    expect(wrapper.classes()).toContain('status-badge--ok')
    expect(wrapper.text()).toContain('ОК')
  })

  it('renders error status', () => {
    const wrapper = mount(StatusBadge, {
      props: {
        status: 'error',
      },
    })

    expect(wrapper.classes()).toContain('status-badge--error')
    expect(wrapper.text()).toContain('Ошибка')
  })

  it('renders warning status', () => {
    const wrapper = mount(StatusBadge, {
      props: {
        status: 'warning',
      },
    })

    expect(wrapper.classes()).toContain('status-badge--warning')
    expect(wrapper.text()).toContain('—')
  })

  it('text prop is accepted but component uses computed text from status', () => {
    const wrapper = mount(StatusBadge, {
      props: {
        status: 'ok',
        text: 'Custom OK',
      },
    })

    // The component uses badgeText computed from status, not the text prop directly
    expect(wrapper.text()).toContain('ОК')
  })

  it('renders slot content when provided', () => {
    const wrapper = mount(StatusBadge, {
      props: {
        status: 'ok',
      },
      slots: {
        default: 'Slot Content',
      },
    })

    expect(wrapper.text()).toBe('Slot Content')
  })

  it('has status-badge class always', () => {
    const wrapper = mount(StatusBadge)

    expect(wrapper.classes()).toContain('status-badge')
  })
})
