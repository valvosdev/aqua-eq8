package http

import (
	"encoding/json"
	"net/http"

	"github.com/golang-jwt/jwt/v5"
	"github.com/labstack/echo/v5"
	"github.com/labstack/gommon/log"
)

func ContextSetterMiddleware() echo.MiddlewareFunc {
	return func(next echo.HandlerFunc) echo.HandlerFunc {
		return func(c *echo.Context) error {
			// 1. Extract the token stored by echojwt (default key is "user")
			token, ok := c.Get("user").(*jwt.Token)
			if !ok || token == nil {
				return echo.NewHTTPError(http.StatusUnauthorized, "Missing or invalid token data")
			}

			// 2. Extract claims as map
			var claims JwtClaims
			claimsBytes, err := json.Marshal(token.Claims)
			if err != nil {
				return echo.NewHTTPError(http.StatusUnauthorized, "Invalid token claims structure")
			}

			if err := json.Unmarshal(claimsBytes, &claims); err != nil {
				return echo.NewHTTPError(http.StatusUnauthorized, "Failed to parse token claims")
			}

			// 3. Populate your domain Metadata struct cleanly using pure type-safe data
			meta := UserMetadata{
				UserID:   claims.UserID,
				TenantID: claims.TenantID,
				Roles:    claims.Roles,
				Extra: map[string]any{
					"name": claims.Name,
				},
			}

			log.Info("Set User Context right now")
			// 4. Inject into standard Go context and pass to the next handler
			ctx := c.Request().Context()
			newCtx := ContextWithUserMetadata(ctx, meta)
			c.SetRequest(c.Request().WithContext(newCtx))

			return next(c)
		}
	}
}
