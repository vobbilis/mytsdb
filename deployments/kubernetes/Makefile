# MyTSDB Kubernetes Deployment Makefile
.PHONY: help preflight install clean status logs-* restart-* update-* shell-* describe-*

# Colors
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
NC := \033[0m # No Color

# Configuration
CLUSTER_NAME := mytsdb-cluster
NAMESPACE := mytsdb
MYTSDB_IMAGE := mytsdb:latest
SAMPLE_APP_IMAGE := sample-app:latest

##@ General

help: ## Display this help
	@echo "$(BLUE)MyTSDB Kubernetes Deployment$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(CYAN)<target>$(NC)\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  $(CYAN)%-20s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Installation

preflight: ## Run pre-flight checks
	@echo "$(BLUE)Running pre-flight checks...$(NC)"
	@./preflight.sh

install: preflight build-all ## Fresh install (with pre-flight checks)
	@echo "$(BLUE)Starting fresh installation...$(NC)"
	@./install.sh

install-force: ## Force reinstall (delete existing cluster)
	@echo "$(YELLOW)Force reinstalling...$(NC)"
	@./install.sh --force

install-skip-preflight: ## Install without pre-flight checks
	@echo "$(YELLOW)Installing (skipping pre-flight)...$(NC)"
	@./install.sh --skip-preflight

##@ Cluster Management

cluster-create: ## Create Kind cluster only
	@echo "$(BLUE)Creating Kind cluster...$(NC)"
	@kind create cluster --config kind/cluster-config.yaml
	@echo "$(GREEN)✓ Cluster created$(NC)"

cluster-delete: ## Delete Kind cluster
	@echo "$(YELLOW)Deleting Kind cluster...$(NC)"
	@kind delete cluster --name $(CLUSTER_NAME)
	@echo "$(GREEN)✓ Cluster deleted$(NC)"

cluster-info: ## Show cluster information
	@kubectl cluster-info --context kind-$(CLUSTER_NAME)

##@ Build & Load

build-mytsdb: ## Build MyTSDB Docker image
	@DOCKER_BUILD_ARGS="$(DOCKER_BUILD_ARGS)" ./manage-images.sh build-mytsdb

build-sample-app: ## Build sample app Docker image
	@./manage-images.sh build-sample-app

build-all: ## Build all Docker images
	@./manage-images.sh build-all

load-mytsdb: ## Load MyTSDB image into Kind
	@./manage-images.sh load-mytsdb

load-sample-app: ## Load sample app image into Kind
	@./manage-images.sh load-sample-app

load-all: ## Load all images into Kind
	@./manage-images.sh load-all

ensure-images: ## Ensure all images are built and loaded (recommended)
	@./manage-images.sh ensure-all

verify-images: ## Verify images are in cluster
	@./manage-images.sh verify

clean-images: ## Clean old/dangling images
	@./manage-images.sh clean

##@ Deployment

deploy-namespace: ## Deploy namespace
	@kubectl apply -f mytsdb/00-namespace.yaml

deploy-mytsdb: ## Deploy MyTSDB
	@echo "$(BLUE)Deploying MyTSDB...$(NC)"
	@kubectl apply -f mytsdb/01-pvc.yaml
	@kubectl apply -f mytsdb/02-deployment.yaml
	@kubectl apply -f mytsdb/03-service.yaml
	@kubectl wait --for=condition=ready pod -l app=mytsdb -n $(NAMESPACE) --timeout=120s
	@echo "$(GREEN)✓ MyTSDB deployed$(NC)"

deploy-otel: ## Deploy OTEL Collector
	@echo "$(BLUE)Deploying OTEL Collector...$(NC)"
	@kubectl apply -f otel-collector/00-configmap.yaml
	@kubectl apply -f otel-collector/01-deployment.yaml
	@kubectl apply -f otel-collector/02-service.yaml
	@kubectl wait --for=condition=ready pod -l app=otel-collector -n $(NAMESPACE) --timeout=60s
	@echo "$(GREEN)✓ OTEL Collector deployed$(NC)"

deploy-sample-app: ## Deploy sample application
	@echo "$(BLUE)Deploying sample app...$(NC)"
	@kubectl apply -f sample-app/00-deployment.yaml
	@kubectl wait --for=condition=ready pod -l app=sample-app -n $(NAMESPACE) --timeout=60s
	@echo "$(GREEN)✓ Sample app deployed$(NC)"

deploy-grafana: ## Deploy Grafana
	@echo "$(BLUE)Deploying Grafana...$(NC)"
	@kubectl apply -f grafana/00-datasource-configmap.yaml
	@kubectl apply -f grafana/01-dashboard-configmap.yaml
	@kubectl apply -f grafana/04-comprehensive-dashboards.yaml
	@kubectl apply -f grafana/02-deployment.yaml
	@kubectl apply -f grafana/03-service.yaml
	@kubectl wait --for=condition=ready pod -l app=grafana -n $(NAMESPACE) --timeout=120s
	@echo "$(GREEN)✓ Grafana deployed$(NC)"

deploy-all: deploy-namespace deploy-mytsdb deploy-otel deploy-sample-app deploy-grafana ## Deploy all components

##@ Updates

update-mytsdb: ## Update MyTSDB (rebuild, load, restart)
	@echo "$(BLUE)Updating MyTSDB...$(NC)"
	@DOCKER_BUILD_ARGS="$(DOCKER_BUILD_ARGS)" ./manage-images.sh build-mytsdb
	@./manage-images.sh force-reload-mytsdb
	@kubectl rollout restart deployment/mytsdb -n $(NAMESPACE)
	@kubectl rollout status deployment/mytsdb -n $(NAMESPACE)
	@echo "$(GREEN)✓ MyTSDB updated$(NC)"

update-otel: ## Update OTEL Collector configuration
	@echo "$(BLUE)Updating OTEL Collector...$(NC)"
	@kubectl apply -f otel-collector/00-configmap.yaml
	@kubectl rollout restart deployment/otel-collector -n $(NAMESPACE)
	@kubectl rollout status deployment/otel-collector -n $(NAMESPACE)
	@echo "$(GREEN)✓ OTEL Collector updated$(NC)"

update-sample-app: ## Update sample application
	@echo "$(BLUE)Updating sample app...$(NC)"
	@./manage-images.sh build-sample-app
	@./manage-images.sh force-reload-sample-app
	@kubectl rollout restart deployment/sample-app -n $(NAMESPACE)
	@kubectl rollout status deployment/sample-app -n $(NAMESPACE)
	@echo "$(GREEN)✓ Sample app updated$(NC)"

update-grafana: ## Update Grafana dashboards/datasources
	@echo "$(BLUE)Updating Grafana...$(NC)"
	@kubectl apply -f grafana/00-datasource-configmap.yaml
	@kubectl apply -f grafana/01-dashboard-configmap.yaml
	@kubectl rollout restart deployment/grafana -n $(NAMESPACE)
	@kubectl rollout status deployment/grafana -n $(NAMESPACE)
	@echo "$(GREEN)✓ Grafana updated$(NC)"

##@ Service Management

restart-mytsdb: ## Restart MyTSDB
	@echo "$(BLUE)Restarting MyTSDB...$(NC)"
	@kubectl rollout restart deployment/mytsdb -n $(NAMESPACE)
	@kubectl rollout status deployment/mytsdb -n $(NAMESPACE)
	@echo "$(GREEN)✓ MyTSDB restarted$(NC)"

restart-otel: ## Restart OTEL Collector
	@echo "$(BLUE)Restarting OTEL Collector...$(NC)"
	@kubectl rollout restart deployment/otel-collector -n $(NAMESPACE)
	@kubectl rollout status deployment/otel-collector -n $(NAMESPACE)
	@echo "$(GREEN)✓ OTEL Collector restarted$(NC)"

restart-sample-app: ## Restart sample application
	@echo "$(BLUE)Restarting sample app...$(NC)"
	@kubectl rollout restart deployment/sample-app -n $(NAMESPACE)
	@kubectl rollout status deployment/sample-app -n $(NAMESPACE)
	@echo "$(GREEN)✓ Sample app restarted$(NC)"

restart-grafana: ## Restart Grafana
	@echo "$(BLUE)Restarting Grafana...$(NC)"
	@kubectl rollout restart deployment/grafana -n $(NAMESPACE)
	@kubectl rollout status deployment/grafana -n $(NAMESPACE)
	@echo "$(GREEN)✓ Grafana restarted$(NC)"

restart-all: restart-mytsdb restart-otel restart-sample-app restart-grafana ## Restart all services

##@ Logs

logs-mytsdb: ## View MyTSDB logs
	@kubectl logs -f -l app=mytsdb -n $(NAMESPACE)

logs-mytsdb-filtered: ## View MyTSDB logs with request tracking
	@kubectl logs -f -l app=mytsdb -n $(NAMESPACE) | grep --line-buffered -E "REQ:|ERROR|WARN|Successfully"

logs-mytsdb-errors: ## View only MyTSDB errors
	@kubectl logs -f -l app=mytsdb -n $(NAMESPACE) | grep --line-buffered "ERROR"

logs-mytsdb-requests: ## View only request logs
	@kubectl logs -f -l app=mytsdb -n $(NAMESPACE) | grep --line-buffered "REQ:"

logs-otel: ## View OTEL Collector logs
	@kubectl logs -f -l app=otel-collector -n $(NAMESPACE)

logs-sample-app: ## View sample app logs
	@kubectl logs -f -l app=sample-app -n $(NAMESPACE)

logs-grafana: ## View Grafana logs
	@kubectl logs -f -l app=grafana -n $(NAMESPACE)

##@ Debugging

debug-requests: ## Show recent requests (last 50 lines)
	@echo "$(BLUE)Recent Requests:$(NC)"
	@kubectl logs --tail=50 -l app=mytsdb -n $(NAMESPACE) | grep "REQ:" || echo "No requests found"

debug-errors: ## Show recent errors
	@echo "$(BLUE)Recent Errors:$(NC)"
	@kubectl logs --tail=100 -l app=mytsdb -n $(NAMESPACE) | grep "ERROR" || echo "No errors found"

debug-performance: ## Show request timing
	@echo "$(BLUE)Request Performance:$(NC)"
	@kubectl logs --tail=100 -l app=mytsdb -n $(NAMESPACE) | grep "Successfully wrote" | grep -oP '\d+ms' || echo "No timing data"

debug-auth: ## Show authentication logs
	@echo "$(BLUE)Authentication Logs:$(NC)"
	@kubectl logs --tail=50 -l app=mytsdb -n $(NAMESPACE) | grep "Authentication" || echo "No auth logs"

debug-all: ## Show comprehensive debug info
	@echo "$(BLUE)=== Pod Status ===$(NC)"
	@kubectl get pods -n $(NAMESPACE)
	@echo ""
	@echo "$(BLUE)=== Recent Requests ===$(NC)"
	@kubectl logs --tail=20 -l app=mytsdb -n $(NAMESPACE) | grep "REQ:" || echo "No requests"
	@echo ""
	@echo "$(BLUE)=== Recent Errors ===$(NC)"
	@kubectl logs --tail=20 -l app=mytsdb -n $(NAMESPACE) | grep "ERROR" || echo "No errors"
	@echo ""
	@echo "$(BLUE)=== Resource Usage ===$(NC)"
	@kubectl top pod -n $(NAMESPACE) 2>/dev/null || echo "Metrics server not available"

##@ Status & Debugging

status: ## Show status of all pods
	@echo "$(BLUE)Pod Status:$(NC)"
	@kubectl get pods -n $(NAMESPACE)
	@echo ""
	@echo "$(BLUE)Services:$(NC)"
	@kubectl get svc -n $(NAMESPACE)
	@echo ""
	@echo "$(BLUE)PVCs:$(NC)"
	@kubectl get pvc -n $(NAMESPACE)

describe-mytsdb: ## Describe MyTSDB pod
	@kubectl describe pod -l app=mytsdb -n $(NAMESPACE)

describe-otel: ## Describe OTEL Collector pod
	@kubectl describe pod -l app=otel-collector -n $(NAMESPACE)

describe-sample-app: ## Describe sample app pod
	@kubectl describe pod -l app=sample-app -n $(NAMESPACE)

describe-grafana: ## Describe Grafana pod
	@kubectl describe pod -l app=grafana -n $(NAMESPACE)

events: ## Show recent events
	@kubectl get events -n $(NAMESPACE) --sort-by='.lastTimestamp'

##@ Shell Access

shell-mytsdb: ## Open shell in MyTSDB pod
	@kubectl exec -it -n $(NAMESPACE) $$(kubectl get pod -l app=mytsdb -n $(NAMESPACE) -o jsonpath='{.items[0].metadata.name}') -- /bin/bash

shell-otel: ## Open shell in OTEL Collector pod
	@kubectl exec -it -n $(NAMESPACE) $$(kubectl get pod -l app=otel-collector -n $(NAMESPACE) -o jsonpath='{.items[0].metadata.name}') -- /bin/sh

shell-sample-app: ## Open shell in sample app pod
	@kubectl exec -it -n $(NAMESPACE) $$(kubectl get pod -l app=sample-app -n $(NAMESPACE) -o jsonpath='{.items[0].metadata.name}') -- /bin/bash

shell-grafana: ## Open shell in Grafana pod
	@kubectl exec -it -n $(NAMESPACE) $$(kubectl get pod -l app=grafana -n $(NAMESPACE) -o jsonpath='{.items[0].metadata.name}') -- /bin/bash

##@ Port Forwarding

forward-mytsdb: ## Port-forward MyTSDB (9090)
	@echo "$(BLUE)Forwarding MyTSDB to localhost:9090$(NC)"
	@kubectl port-forward -n $(NAMESPACE) svc/mytsdb 9090:9090

forward-grafana: ## Port-forward Grafana (3000)
	@echo "$(BLUE)Forwarding Grafana to localhost:3000$(NC)"
	@kubectl port-forward -n $(NAMESPACE) svc/grafana 3000:3000

forward-otel: ## Port-forward OTEL Collector (4317, 4318)
	@echo "$(BLUE)Forwarding OTEL Collector to localhost:4317,4318$(NC)"
	@kubectl port-forward -n $(NAMESPACE) svc/otel-collector 4317:4317 4318:4318

##@ Scaling

scale-mytsdb: ## Scale MyTSDB (usage: make scale-mytsdb REPLICAS=2)
	@kubectl scale deployment mytsdb -n $(NAMESPACE) --replicas=$(REPLICAS)
	@echo "$(GREEN)✓ MyTSDB scaled to $(REPLICAS) replicas$(NC)"

scale-sample-app: ## Scale sample app (usage: make scale-sample-app REPLICAS=3)
	@kubectl scale deployment sample-app -n $(NAMESPACE) --replicas=$(REPLICAS)
	@echo "$(GREEN)✓ Sample app scaled to $(REPLICAS) replicas$(NC)"

##@ Cleanup

clean-namespace: ## Delete namespace and all resources
	@echo "$(YELLOW)Deleting namespace...$(NC)"
	@kubectl delete namespace $(NAMESPACE) --ignore-not-found=true
	@echo "$(GREEN)✓ Namespace deleted$(NC)"

clean: cluster-delete ## Full cleanup (delete cluster)
	@echo "$(GREEN)✓ Cleanup complete$(NC)"

##@ Testing

test-mytsdb-health: ## Test MyTSDB health endpoint
	@echo "$(BLUE)Testing MyTSDB health...$(NC)"
	@kubectl run curl-test --image=curlimages/curl:latest --rm -i --restart=Never -n $(NAMESPACE) -- \
		curl -s http://mytsdb.$(NAMESPACE).svc.cluster.local:9090/health

test-connectivity: ## Test connectivity between services
	@echo "$(BLUE)Testing service connectivity...$(NC)"
	@echo "MyTSDB → OTEL:"
	@kubectl run curl-test --image=curlimages/curl:latest --rm -i --restart=Never -n $(NAMESPACE) -- \
		curl -s http://mytsdb.$(NAMESPACE).svc.cluster.local:9090/health
	@echo ""
	@echo "OTEL → MyTSDB: Check OTEL logs for successful exports"

##@ Quick Actions

quick-status: ## Quick status check
	@echo "$(BLUE)Quick Status:$(NC)"
	@kubectl get pods -n $(NAMESPACE) | grep -E "NAME|mytsdb|otel|sample|grafana"

quick-logs: ## Tail all logs (last 10 lines each)
	@echo "$(BLUE)=== MyTSDB ===$(NC)"
	@kubectl logs --tail=10 -l app=mytsdb -n $(NAMESPACE) 2>/dev/null || echo "No logs"
	@echo ""
	@echo "$(BLUE)=== OTEL Collector ===$(NC)"
	@kubectl logs --tail=10 -l app=otel-collector -n $(NAMESPACE) 2>/dev/null || echo "No logs"
	@echo ""
	@echo "$(BLUE)=== Sample App ===$(NC)"
	@kubectl logs --tail=10 -l app=sample-app -n $(NAMESPACE) 2>/dev/null || echo "No logs"

watch-pods: ## Watch pod status (Ctrl+C to exit)
	@watch -n 2 kubectl get pods -n $(NAMESPACE)
