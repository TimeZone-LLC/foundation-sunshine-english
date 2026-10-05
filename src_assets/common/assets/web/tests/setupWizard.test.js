import test from 'node:test'
import assert from 'node:assert/strict'
import { useSetupWizard } from '../composables/useSetupWizard.js'

test('setup wizard state is independent of browser and saved locale data', () => {
  Object.defineProperty(globalThis, 'navigator', {
    value: { languages: ['zh-CN'], language: 'zh-CN' },
    configurable: true,
  })

  const wizard = useSetupWizard()
  assert.equal(wizard.checkSetupWizard({
    setup_wizard_completed: false,
    locale: 'zh',
    adapters: ['adapter'],
    display_devices: ['display'],
  }), true)

  assert.equal('hasLocale' in wizard, false)
  assert.deepEqual(wizard.adapters.value, ['adapter'])
  assert.deepEqual(wizard.displayDevices.value, ['display'])
})

test('completed setup does not reopen the wizard', () => {
  const wizard = useSetupWizard()

  assert.equal(wizard.checkSetupWizard({ setup_wizard_completed: true }), false)
  assert.equal(wizard.showSetupWizard.value, false)
})
