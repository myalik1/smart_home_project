import json
import threading
from fastapi import APIRouter, HTTPException, status

from gateway.config import settings
from gateway.schemas import RuleItemSchema, RulesConfigSchema

router = APIRouter(prefix="/rules", tags=["Automation Rules"])

_rules_file_lock = threading.Lock()


def _load_rules_from_disk() -> RulesConfigSchema:
    path = settings.rules_full_path
    if not path.exists():
        return RulesConfigSchema(rules=[])
    try:
        raw_text = path.read_text(encoding="utf-8")
        if not raw_text.strip():
            return RulesConfigSchema(rules=[])
        data = json.loads(raw_text)
        return RulesConfigSchema.model_validate(data)
    except Exception as exc:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to read or parse rules configuration file: {exc}",
        ) from exc


def _save_rules_to_disk(config: RulesConfigSchema) -> None:
    path = settings.rules_full_path
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = config.model_dump(mode="json", exclude_none=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")


@router.get(
    "",
    response_model=RulesConfigSchema,
    summary="Получить текущую конфигурацию правил и таймеров",
)
async def get_rules_config() -> RulesConfigSchema:
    with _rules_file_lock:
        return _load_rules_from_disk()


@router.put(
    "",
    response_model=RulesConfigSchema,
    summary="Полностью перезаписать конфигурацию правил",
)
async def replace_rules_config(config: RulesConfigSchema) -> RulesConfigSchema:
    seen_ids = set()
    for item in config.rules:
        if item.id in seen_ids:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Duplicate rule id '{item.id}' in payload.",
            )
        seen_ids.add(item.id)

    with _rules_file_lock:
        _save_rules_to_disk(config)
        return config


@router.post(
    "/items",
    response_model=RuleItemSchema,
    status_code=status.HTTP_201_CREATED,
    summary="Добавить одно новое правило автоматизации",
)
async def add_rule_item(rule: RuleItemSchema) -> RuleItemSchema:
    with _rules_file_lock:
        config = _load_rules_from_disk()
        if any(existing.id == rule.id for existing in config.rules):
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail=f"Rule with id '{rule.id}' already exists.",
            )
        config.rules.append(rule)
        _save_rules_to_disk(config)
        return rule


@router.delete(
    "/items/{rule_id}",
    response_model=RulesConfigSchema,
    summary="Удалить правило автоматизации по его ID",
)
async def delete_rule_item(rule_id: str) -> RulesConfigSchema:
    with _rules_file_lock:
        config = _load_rules_from_disk()
        initial_len = len(config.rules)
        config.rules = [r for r in config.rules if r.id != rule_id]

        if len(config.rules) == initial_len:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Rule with id '{rule_id}' not found.",
            )

        _save_rules_to_disk(config)
        return config
