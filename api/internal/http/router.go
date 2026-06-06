package http

import (
	"net/http"

	echojwt "github.com/labstack/echo-jwt/v5"
	echoprometheus "github.com/labstack/echo-prometheus"
	"github.com/labstack/echo/v5"
	"github.com/labstack/echo/v5/middleware"
	"github.com/valvosdev/aqua-eq8/api/infrastructure/config"

	khttp "github.com/valvosdev/aqua-eq8/api/infrastructure/http"

	"go.uber.org/fx"
)

type RouterParams struct {
	fx.In // Tells fx to inject fields by type matching

	Echo         *echo.Echo
	Cfg          *config.Config
	HttpHandlers []HttpHandler `group:"handlers"`
}

func NewHttpRouter(p RouterParams) {
	e := p.Echo
	e.Use(middleware.RequestLogger())
	e.Use(middleware.Recover())
	e.Use(khttp.CorrelationID())

	e.HTTPErrorHandler = khttp.HTTPErrorHandler

	if p.Cfg.Metrics.Enabled {
		e.Use(echoprometheus.NewMiddleware("aqua-eq8-api"))
		// TODO: check if requires a secured endpoint
		e.GET("/metrics", echoprometheus.NewHandler()) // expose metrics for scraping
	}

	if p.Cfg.Swagger.Enabled {
		//e.GET("/swagger/*", echoSwagger.WrapHandler)
	}

	e.GET("/health", func(c *echo.Context) error {
		return c.JSON(http.StatusOK, map[string]any{
			"message": "healthy",
			"status":  http.StatusOK,
		})
	})

	e.GET("/ping", func(c *echo.Context) error {
		return c.String(http.StatusOK, "pong")
	})

	v1g := e.Group("/api/v1")
	publicGroup := v1g.Group("")
	jwtMiddleware := echojwt.WithConfig(echojwt.Config{
		SigningKey: []byte(p.Cfg.JWT.Secret),
		//TokenLookup: "header:x-auth-token",
	})
	protectedGroup := v1g.Group("", jwtMiddleware, khttp.ContextSetterMiddleware())

	groups := RouterGroups{
		Public:    publicGroup,
		Protected: protectedGroup,
	}

	// v1 http handlers register them self
	for _, handler := range p.HttpHandlers {
		handler.RegisterRoutes(groups)
	}
}

func AsHTTPHandler(f any) any {
	return fx.Annotate(f, fx.As(new(HttpHandler)), fx.ResultTags(`group:"handlers"`))
}
