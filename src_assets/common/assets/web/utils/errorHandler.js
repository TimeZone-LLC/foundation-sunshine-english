import { APP_CONSTANTS } from './constants.js';

/**
 * 统一错误处理类
 */
export class ErrorHandler {
  /**
   * 处理网络错误
   * @param {Error} error 错误对象
   * @param {string} context 错误上下文
   * @returns {string} 用户友好的错误信息
   */
  static handleNetworkError(error, context = 'Operation') {
    console.error(`${context} failed:`, error);
    
    if (error.name === 'TypeError' && error.message.includes('Failed to fetch')) {
      return `Network connection failed. Check your connection and try again`;
    }
    
    if (error.message.includes('404')) {
      return `${context} failed: the requested resource does not exist`;
    }
    
    if (error.message.includes('500')) {
      return `${context} failed: an internal server error occurred`;
    }
    
    if (error.message.includes('403')) {
      return `${context} failed: insufficient permissions`;
    }
    
    return `${context} failed: ${error.message || 'Unknown error'}`;
  }

  /**
   * 处理验证错误
   * @param {Array} errors 错误数组
   * @returns {string} 格式化的错误信息
   */
  static handleValidationErrors(errors) {
    if (!Array.isArray(errors) || errors.length === 0) {
      return 'Validation failed';
    }
    
    return errors.join('; ');
  }

  /**
   * 处理应用操作错误
   * @param {Error} error 错误对象
   * @param {string} operation 操作类型
   * @param {string} appName 应用名称
   * @returns {string} 格式化的错误信息
   */
  static handleAppError(error, operation, appName = '') {
    const appContext = appName ? ` "${appName}"` : '';
    
    switch(operation) {
      case 'save':
        return this.handleNetworkError(error, `Saving app${appContext}`);
      case 'delete':
        return this.handleNetworkError(error, `Deleting app${appContext}`);
      case 'load':
        return this.handleNetworkError(error, `Loading app${appContext}`);
      default:
        return this.handleNetworkError(error, `Updating app${appContext}`);
    }
  }

  /**
   * 创建错误弹窗
   * @param {string} message 错误信息
   * @param {string} title 标题
   */
  static showErrorDialog(message, title = 'Error') {
    // 如果需要更复杂的错误弹窗，可以在这里实现
    // 目前使用简单的 alert
    alert(`${title}\n\n${message}`);
  }

  /**
   * 创建确认弹窗
   * @param {string} message 确认信息
   * @param {string} title 标题
   * @returns {boolean} 用户是否确认
   */
  static showConfirmDialog(message, title = 'Confirm') {
    return confirm(`${title}\n\n${message}`);
  }

  /**
   * 记录错误日志
   * @param {Error} error 错误对象
   * @param {string} context 错误上下文
   * @param {Object} metadata 额外的元数据
   */
  static logError(error, context = '', metadata = {}) {
    const errorInfo = {
      message: error.message,
      stack: error.stack,
      context,
      timestamp: new Date().toISOString(),
      ...metadata
    };
    
    console.error('Application error:', errorInfo);
    
    // 如果需要发送到日志服务，可以在这里实现
    // this.sendToLogService(errorInfo);
  }

  /**
   * 处理异步操作错误
   * @param {Promise} promise Promise对象
   * @param {string} context 错误上下文
   * @returns {Promise} 包装后的Promise
   */
  static async handleAsyncError(promise, context = '') {
    try {
      return await promise;
    } catch (error) {
      this.logError(error, context);
      throw error;
    }
  }
} 