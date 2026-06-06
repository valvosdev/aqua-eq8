package v1

import (
	"time"

	nhttp "net/http"

	"github.com/golang-jwt/jwt/v5"
	"github.com/valvosdev/aqua-eq8/api/infrastructure/config"
	khttp "github.com/valvosdev/aqua-eq8/api/infrastructure/http"
	"github.com/valvosdev/aqua-eq8/api/internal/http"
	"github.com/valvosdev/aqua-eq8/api/internal/usecases/authentication"

	"github.com/labstack/echo/v5"
)

type AuthHandler struct {
	service authentication.AuthService
	cfg     *config.Config
}

func NewAuthHandler(cfg *config.Config, s authentication.AuthService) *AuthHandler {
	return &AuthHandler{
		service: s,
		cfg:     cfg,
	}
}

func (h *AuthHandler) RegisterRoutes(groups http.RouterGroups) {
	authGroup := groups.Public.Group("/auth")
	{
		authGroup.POST("/register", h.Register)
		authGroup.POST("/login", h.Login)
	}
}

func (h *AuthHandler) Login(c *echo.Context) error {
	loginRequest := struct {
		Username string `json:"username"`
		Password string `json:"password"`
	}{}

	if err := c.Bind(&loginRequest); err != nil {
		return err
	}

	ctx := c.Request().Context()
	res, err := h.service.Login(ctx, loginRequest.Username, loginRequest.Password)
	if err != nil {
		return err
	}

	// set the claims
	claims := khttp.JwtClaims{
		UserID:   res.UserID,
		TenantID: res.TenantID,
		Name:     res.Fullname,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Hour * h.cfg.JWT.TokenExpiry)),
		},
	}
	claims.Roles = res.Roles

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	t, err := token.SignedString([]byte(h.cfg.JWT.Secret))
	if err != nil {
		return err
	}

	return c.JSON(nhttp.StatusOK, map[string]string{
		"token": t,
	})
}

func (h *AuthHandler) Register(c *echo.Context) error {

	return c.String(nhttp.StatusOK, "registration done")
}
