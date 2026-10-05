/**
 * Compatibility-only analytics facade.
 *
 * This self-hosted fork does not initialize a third-party SDK, schedule
 * analytics work, persist identifiers, or transmit usage data. The no-op
 * surface remains so feature code can report local actions without needing
 * telemetry-specific branches.
 */
export const analytics = null

export function initFirebase() {
  return Promise.resolve(null)
}

export function trackEvent() {}

export const trackEvents = Object.freeze({
  pageView: () => {},
  userAction: () => {},
  appAdded: () => {},
  appDeleted: () => {},
  configChanged: () => {},
  errorOccurred: () => {},
  devicePaired: () => {},
})
