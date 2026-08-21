export const LOG_REMEDIATION_SKILL_ID = 'diagnostics.logs.remediation'

const REMEDIATION_BY_TYPE = {
  'encoder-failure': {
    severity: 'error',
    title: 'Check encoder availability and GPU driver support',
    labels: { zh: 'Check encoder availability and GPU driver support' },
    actions: [
      'Confirm the selected encoder is supported by the current GPU and driver.',
      'Try switching to another hardware encoder or software encoding as a temporary fallback.',
      'Update the GPU driver, then restart Sunshine.',
    ],
    actionLabels: {
      zh: [
        'Confirm the selected encoder is supported by the current GPU and driver.',
        'Try switching to another hardware encoder or software encoding as a temporary fallback.',
        'Update the GPU driver, then restart Sunshine.',
      ],
    },
  },
  'display-capture-failure': {
    severity: 'error',
    title: 'Verify display selection and capture availability',
    labels: { zh: 'Verify display selection and capture availability' },
    actions: [
      'Open the Troubleshooting page and reset display device persistence if the display changed recently.',
      'Confirm the selected monitor is connected, enabled, and visible to the system.',
      'Restart Sunshine after changing display or GPU settings.',
    ],
    actionLabels: {
      zh: [
        'Open the Troubleshooting page and reset display device persistence if the display changed recently.',
        'Confirm the selected monitor is connected, enabled, and visible to the system.',
        'Restart Sunshine after changing display or GPU settings.',
      ],
    },
  },
  'network-timeout': {
    severity: 'warning',
    title: 'Check client connectivity, firewall, and network path',
    labels: { zh: 'Check client connectivity, firewall, and network path' },
    actions: [
      'Confirm the Moonlight client can reach the Sunshine host IP.',
      'Check firewall rules for Sunshine and the configured streaming ports.',
      'Retry pairing after confirming both devices are on the expected network.',
    ],
    actionLabels: {
      zh: [
        'Confirm the Moonlight client can reach the Sunshine host IP.',
        'Check firewall rules for Sunshine and the configured streaming ports.',
        'Retry pairing after confirming both devices are on the expected network.',
      ],
    },
  },
  'port-bind-failure': {
    severity: 'error',
    title: 'Free or change the occupied Sunshine port',
    labels: { zh: 'Free or change the occupied Sunshine port' },
    actions: [
      'Check whether another Sunshine instance or service is already listening on the same port.',
      'Stop the conflicting process or change the Sunshine port configuration.',
      'Run Sunshine with sufficient permissions if the log mentions permission denied.',
    ],
    actionLabels: {
      zh: [
        'Check whether another Sunshine instance or service is already listening on the same port.',
        'Stop the conflicting process or change the Sunshine port configuration.',
        'Run Sunshine with sufficient permissions if the log mentions permission denied.',
      ],
    },
  },
  'config-risk': {
    severity: 'warning',
    title: 'Review recently changed Sunshine configuration',
    labels: { zh: 'Review recently changed Sunshine configuration' },
    actions: [
      'Review the setting mentioned near the warning or error line.',
      'Restore the option to a known working value if the issue started after a config change.',
      'Save the config and restart Sunshine after changing related settings.',
    ],
    actionLabels: {
      zh: [
        'Review the setting mentioned near the warning or error line.',
        'Restore the option to a known working value if the issue started after a config change.',
        'Save the config and restart Sunshine after changing related settings.',
      ],
    },
  },
}

export function createRemediationSuggestions(findings = []) {
  const suggestionsByType = new Map()

  for (const finding of findings) {
    const template = REMEDIATION_BY_TYPE[finding?.type]
    if (!template || suggestionsByType.has(finding.type)) continue

    suggestionsByType.set(finding.type, {
      id: `remediation:${finding.type}`,
      type: 'remediation',
      findingType: finding.type,
      category: finding.category || 'general',
      severity: template.severity || finding.severity || 'info',
      skillId: LOG_REMEDIATION_SKILL_ID,
      title: template.title,
      labels: template.labels || {},
      actions: template.actions || [],
      actionLabels: template.actionLabels || {},
      evidence: finding.evidence || [],
    })
  }

  return Array.from(suggestionsByType.values())
}

export function createLogRemediationSkill(options = {}) {
  const createSuggestions = options.createSuggestions || createRemediationSuggestions

  return {
    id: LOG_REMEDIATION_SKILL_ID,
    type: 'log-analysis',
    label: 'Log remediation suggestions',

    async run(context) {
      const suggestions = createSuggestions(context.findings || [])

      context.events?.push({
        skillId: LOG_REMEDIATION_SKILL_ID,
        type: 'logs:remediation-suggested',
        suggestionsFound: suggestions.length,
      })

      return {
        ...context,
        suggestions: [...(context.suggestions || []), ...suggestions],
        stats: {
          ...(context.stats || {}),
          logRemediationSuggestions: suggestions.length,
        },
      }
    },
  }
}
