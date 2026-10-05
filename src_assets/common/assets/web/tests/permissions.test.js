import test from 'node:test'
import assert from 'node:assert/strict'
import { nextTick } from 'vue'
import { orderPermissions, usePermissions } from '../composables/usePermissions.js'

test('permission rows keep the macOS access sequence stable', () => {
  assert.deepEqual(
    orderPermissions([
      { id: 'local_network' },
      { id: 'microphone' },
      { id: 'screen_recording' },
      { id: 'input' },
    ]).map((permission) => permission.id),
    ['screen_recording', 'input', 'microphone', 'local_network']
  )
})

test('permission state reports only verifiable required access as missing', async () => {
  const state = usePermissions({
    fetchStatuses: async () => ({
      permissions: [
        { id: 'screen_recording', status: 'denied', required: true, verifiable: true },
        { id: 'microphone', status: 'denied', required: false, verifiable: true },
        { id: 'local_network', status: 'on_use', required: true, verifiable: false },
      ],
    }),
  })

  await state.refreshPermissions()
  await nextTick()
  assert.deepEqual(state.missingRequired.value.map((permission) => permission.id), ['screen_recording'])
})

test('permission requests refresh status and release the busy state', async () => {
  const requests = []
  let granted = false
  const state = usePermissions({
    fetchStatuses: async () => ({
      permissions: [{
        id: 'microphone',
        status: granted ? 'granted' : 'not_determined',
        required: true,
        verifiable: true,
      }],
    }),
    requestAccess: async (id) => {
      requests.push(id)
      granted = true
      return { status: true }
    },
  })

  assert.equal(await state.requestPermission('microphone'), true)
  assert.deepEqual(requests, ['microphone'])
  assert.equal(state.permissions.value[0].status, 'granted')
  assert.equal(state.busyPermission.value, '')
})
