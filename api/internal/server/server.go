package server

import (
	"context"
	"errors"

	nhttp "net/http"
	"time"

	"github.com/labstack/echo/v5"
	"github.com/valvosdev/aqua-eq8/api/infrastructure/config"
	"github.com/valvosdev/aqua-eq8/api/internal/http"
	v1 "github.com/valvosdev/aqua-eq8/api/internal/http/v1"
	"go.uber.org/fx"
	"gorm.io/gorm"
)

type Server struct {
	cfg  *config.Config
	echo *echo.Echo
	db   *gorm.DB
}

var Module = fx.Options(
	// 1. Framework code primitives
	fx.Provide(
		echo.New,
	),

	// 2. Register HTTP Deliveries as auto-scanned handlers
	fx.Provide(
		http.AsHTTPHandler(v1.NewAuthHandler),
	),
	// 3. Map paths and intercept system hooks for elegant shutdown
	fx.Invoke(
		http.NewHttpRouter,
		RegisterLifecycle,
	),
)

func RegisterLifecycle(lc fx.Lifecycle, e *echo.Echo, cfg *config.Config) {
	// Create a cancelable context that we control inside the Fx lifecycle
	serverCtx, cancelServer := context.WithCancel(context.Background())

	lc.Append(fx.Hook{
		OnStart: func(ctx context.Context) error {
			// Echo v5 config layout managing graceful shutdowns natively
			sc := echo.StartConfig{
				Address:         cfg.HTTP.Port,
				GracefulTimeout: 5 * time.Second, // Drains in-flight requests cleanly
			}

			go func() {
				// We pass our managed context down into Start.
				// When cancelServer() is executed on stop, it triggers graceful teardown.
				if err := sc.Start(serverCtx, e); err != nil && !errors.Is(err, nhttp.ErrServerClosed) {
					e.Logger.Error("Echo server crashed during execution: ", err)
				}
			}()
			return nil
		},
		OnStop: func(ctx context.Context) error {
			// Trigger Echo v5's internal graceful stop routine by cancelling its runtime context
			cancelServer()

			// Give Echo a brief window to complete draining the connections before Fx exits completely
			time.Sleep(500 * time.Millisecond)
			return nil
		},
	})
}

// TODO: create an Options class and send it to Server instead on lots on params
func NewServer(cfg *config.Config, db *gorm.DB) *Server {
	return &Server{
		cfg:  cfg,
		db:   db,
		echo: echo.New()}
}
