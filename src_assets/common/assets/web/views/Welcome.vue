<template>
  <div id="content" class="container welcome-page">
    <div class="row justify-content-center my-4">
      <div class="col-lg-10">
        <div class="card my-4">
          <div class="card-body">
            <header class="text-center mb-4">
              <h1 class="mb-3">
                {{ $t('welcome.greeting') }}
              </h1>
              <p class="lead text-muted">{{ $t('welcome.create_creds') }}</p>
            </header>

            <div class="alert alert-warning">
              <i class="fas fa-exclamation-triangle me-2"></i>
              {{ $t('welcome.create_creds_alert') }}
              <br>
              <i class="fas fa-shield-alt me-2 mt-2"></i>
              {{ $t('welcome.creds_local_only') }}
            </div>

            <form @submit.prevent="save">
              <div class="row justify-content-center">
                <div class="col-md-8">
                  <div class="mb-3">
                    <label for="usernameInput" class="form-label">
                      <i class="fas fa-user me-2"></i>{{ $t('welcome.username') }}
                    </label>
                    <input
                      type="text"
                      class="form-control"
                      id="usernameInput"
                      autocomplete="username"
                      v-model="passwordData.newUsername"
                      :placeholder="$t('welcome.username')"
                    />
                  </div>

                  <div class="mb-3">
                    <label for="passwordInput" class="form-label">
                      <i class="fas fa-lock me-2"></i>{{ $t('welcome.password') }}
                    </label>
                    <input
                      type="password"
                      class="form-control"
                      id="passwordInput"
                      autocomplete="new-password"
                      v-model="passwordData.newPassword"
                      :placeholder="$t('welcome.password')"
                      required
                    />
                  </div>

                  <div class="mb-3">
                    <label for="confirmPasswordInput" class="form-label">
                      <i class="fas fa-check-circle me-2"></i>{{ $t('welcome.confirm_password') }}
                    </label>
                    <input
                      type="password"
                      class="form-control"
                      :class="{ 'is-invalid': !passwordsMatch && passwordData.confirmNewPassword }"
                      id="confirmPasswordInput"
                      autocomplete="new-password"
                      v-model="passwordData.confirmNewPassword"
                      :placeholder="$t('welcome.confirm_password')"
                      required
                    />
                    <div class="invalid-feedback d-block" v-if="!passwordsMatch && passwordData.confirmNewPassword">
                      <i class="fas fa-exclamation-circle me-1"></i>{{ $t('welcome.password_mismatch') }}
                    </div>
                    <div
                      class="valid-feedback d-block"
                      v-if="passwordsMatch && passwordData.confirmNewPassword && passwordData.newPassword"
                    >
                      <i class="fas fa-check-circle me-1"></i>{{ $t('welcome.password_match') }}
                    </div>
                  </div>

                  <button type="submit" class="btn btn-primary w-100 mb-3" :disabled="loading || !isFormValid">
                    <span
                      v-if="loading"
                      class="spinner-border spinner-border-sm me-2"
                      role="status"
                      aria-hidden="true"
                    ></span>
                    <i v-else class="fas fa-sign-in-alt me-2"></i>
                    {{ $t('welcome.login') }}
                  </button>

                  <transition name="fade">
                    <div class="alert alert-danger" v-if="error">
                      <i class="fas fa-exclamation-circle me-2"></i>
                      <strong>{{ $t('welcome.error') }}</strong>
                      <span v-if="error.startsWith('welcome.')">{{ $t(error) }}</span>
                      <span v-else>{{ error }}</span>
                    </div>
                  </transition>

                  <transition name="fade">
                    <div class="alert alert-success" v-if="success">
                      <i class="fas fa-check-circle me-2"></i>
                      <strong>{{ $t('welcome.success') }}</strong> {{ $t('welcome.welcome_success') }}
                    </div>
                  </transition>
                </div>
              </div>
            </form>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { useWelcome } from '../composables/useWelcome.js'
import { loadAutoTheme } from '../utils/theme.js'

const { error, success, loading, passwordData, passwordsMatch, isFormValid, save } = useWelcome()

loadAutoTheme()
</script>

<style scoped>
/*
 * Welcome / first-run credentials page.
 *
 * This page used to be styled as a hand-drawn sketch on paper: --sketch-*
 * alias variables, a blurred card, a drop-shadowed logo, a pill underline and
 * shake / errorPop / successPop / sketchIn / sketchOut keyframes. All of it is
 * gone; the page is now the same flat, framed surface as every other view and
 * reads its colors straight from the token layer.
 */
#content {
  position: relative;
  z-index: 1;
  padding-top: 1rem;
  padding-bottom: 2rem;
}

.card {
  background: var(--ui-surface);
  border: 1px solid var(--ui-border);
  border-radius: 0;
}

.card-body {
  padding: clamp(1.25rem, 4vw, 2.5rem);
}

h1 {
  color: var(--ui-text-primary);
  font-weight: var(--ui-label-weight);
  text-transform: var(--ui-label-transform);
  letter-spacing: var(--ui-label-tracking);
}

.lead {
  color: var(--ui-text-secondary);
  font-size: 1.1rem;
}

.alert-warning {
  background: var(--ui-warning-soft);
  border: 1px solid var(--ui-warning-border);
  border-radius: 0;
  color: var(--ui-warning-text);
  font-size: 0.95rem;
  font-weight: 500;
  padding: 1rem 1.25rem;
}

.alert-warning i {
  color: var(--ui-warning-text);
}

.form-label {
  color: var(--ui-text-primary);
  font-size: 0.95rem;
  font-weight: 600;
}

.form-label i {
  color: var(--ui-text-muted);
  margin-right: 0.3rem;
}

.form-control {
  background: var(--ui-surface-strong);
  border: 1px solid var(--ui-border);
  border-radius: 0;
  padding: 12px 16px;
  font-size: 1rem;
  color: var(--ui-text-primary);
  transition: var(--transition-default);
}

.form-control:focus {
  border-color: var(--ui-border-strong);
  background: var(--ui-surface-hover);
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.form-control::placeholder {
  color: var(--ui-text-muted);
}

.form-control.is-invalid {
  border-color: var(--ui-danger);
  background: var(--ui-danger-soft);
}

.invalid-feedback,
.valid-feedback {
  display: block;
  margin-top: 0.5rem;
  font-size: 0.9rem;
  font-weight: 600;
}

.invalid-feedback {
  color: var(--ui-danger-text);
}

.valid-feedback {
  color: var(--ui-success-text);
}

.btn-primary {
  background: var(--ui-accent);
  border: 1px solid var(--ui-accent);
  border-radius: 0;
  padding: 12px 28px;
  font-size: 1rem;
  font-weight: var(--ui-label-weight);
  text-transform: var(--ui-label-transform);
  letter-spacing: var(--ui-label-tracking);
  color: var(--ui-accent-contrast);
  transition: var(--transition-default);
}

.btn-primary:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.btn-primary:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  background: var(--ui-surface-strong);
  border-color: var(--ui-border-strong);
  color: var(--ui-text-muted);
}

/* Genuine loading feedback: Bootstrap's plain rotation, recolored only. */
.spinner-border {
  border-color: var(--ui-border-strong);
  border-right-color: currentColor;
}

.alert-danger {
  background: var(--ui-danger-soft);
  border: 1px solid var(--ui-danger-border);
  border-radius: 0;
  color: var(--ui-danger-text);
}

.alert-success {
  background: var(--ui-success-soft);
  border: 1px solid var(--ui-success-border);
  border-radius: 0;
  color: var(--ui-success-text);
}

@media (max-width: 768px) {
  .card {
    margin: 1rem 0.5rem;
  }

  h1 {
    font-size: 1.5rem;
  }

  .btn-primary {
    font-size: 1rem;
    padding: 12px 24px;
  }

  .col-md-8 {
    padding-left: 0.5rem;
    padding-right: 0.5rem;
  }
}
</style>
