package http

import (
	"context"

	"github.com/google/uuid"
	"github.com/labstack/echo/v5"
)

const (
	CorrelationIDHeader = "X-Correlation-ID"
	CorrelationIDKey    = "correlation_id"
)

func CorrelationID() echo.MiddlewareFunc {

	return func(next echo.HandlerFunc) echo.HandlerFunc {
		return func(c *echo.Context) error {
			correlationID := c.Request().Header.Get(CorrelationIDHeader)
			if correlationID == "" {
				correlationID = uuid.NewString()
			}
			// Store in Echo context
			c.Set(CorrelationIDKey, correlationID)
			// Store in standard request context
			ctx := context.WithValue(
				c.Request().Context(),
				CorrelationIDKey,
				correlationID,
			)
			req := c.Request().WithContext(ctx)
			c.SetRequest(req)
			// Add to response
			c.Response().Header().Set(
				CorrelationIDHeader,
				correlationID,
			)
			return next(c)
		}
	}
}
